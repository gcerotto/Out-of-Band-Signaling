# Out-of-Band Signaling
Progetto realizzato per il corso di sistemi operativi. Lo scopo del progetto è realizzare un insieme di programmi che si comunicano un segreto tramite out-of-band signaling, seguendo le specifiche riportate nel file CONSEGNA.md

## Client
Inizializza il PRNG random() con il nanosecondo corrente, ottenuto dall'orologio di sistema: una precisione al secondo non sarebbe stata sufficiente ad evitare un seme identico per diversi client.

L'id di 64 bit del client è ottenuta riempiedo di dati casuali i primi 32 bit e poi i secondi, perché random() restituisce un long, di lunghezza minima 32 bit. È stata utilizzata una union a questo scopo, ma poteva essere usato un puntatore a uint32_t (assegnato al riferimento del uint64) o uno shift di 32 bit.
Per comunicare con n diversi server sono inizializzati 3 array di dimensione n contenenti: l'id del server a cui collegarsi, la struttura sockaddr_un, il file descriptor.

Gli id sono scelti dalla funzione fill_array, che genera casualmente un candidato e lo inserisce solo nel caso in cui non sia già presente nell'array, per evitare connessioni doppie allo stesso server.

Per ogni elemento dell'array degli id, sono inizializzate adeguatamente le strutture sockaddr_un, ed eseguite le chiamate di sistema socket() e connect()
Successivamente, ogni x millisecondi, è scelto un file descriptor a caso tra quelli disponibili per inviare i dati.


## Supervisor
Setta il buffer alla modalità line buffered, che verrà ereditata dai server e permette di non perdere messaggi all'arrivo del SIGTERM quando l'output è reindirizzato su file.

È creata una pipe il cui lato di scrittura è dato ai server creati tramite fork(). Sul lato di lettura è settato il flag close-on-exec.
Le stime che arrivano dai server sono inserite in una lista i cui nodi contengono: l'id del client e un array con le stime del secret (aggiornato ad ogni inserimento per uno stesso id), assieme alla sua dimensione, che indica anche il numero di server che hanno fornito una stima.

Il segnale SIGINT è mascherato per permettere ad un thread dedicato di intercettarlo. La maschera dei segnali è ereditata dal server, che non rischia di essere terminato accidentalmente

Per garantire l'integrità dei dati, l'accesso alla lista da parte del thread che inserisce i dati e da quello che li stampa all'arrivo del SIGINT è regolato da una mutex.

Il segreto è stimato calcolando la moda: questo perché i server che hanno abbastanza informazioni inviano valori esatti, mentre uno o più server potrebbero dare una stima con grande errore, e calcolare la media avrebbe l'effetto di distribuire questo errore.


## Server
Ho scelto di creare un nuovo thread per ogni connessione in ingresso invece di usare select() per semplicità di programmazione e perché deve essere valutata con precisione la differenza di tempo tra un messaggio e l'altro, quindi il piccolo costo iniziale della creazione del thread è trascurabile.

Il thread pool non avrebbe alcun vantaggio, dal momento che il numero di connessioni attive è sconosciuto e le richieste sono tutt'altro che brevi, e select() da prove fatte non ha alcun vantaggio sul paradigma thread con read() bloccante.

Il segnale SIGPIPE è bloccato per permettere al processo di uscire cancellando il socket dal filesystem. Il thread scrive in un array che cresce dinamicamente il tempo di arrivo e calcola le differenze tra tempo t e t+1.

I numeri ottenuti sono nella forma a*x+e, dove x è il secret, a un intero ed e l'errore. a dipende dall'ordine con cui il client contatta i server.
È selezionato il numero minore min, su ogni elemento dell'array el è compiuta l'operazione mod(el, min) per “appiattire” la differenza tra le misurazioni (finché questa operazione non da valori prossimi a zero). Min è aggiornato ad ogni ciclo.
```
mod(ax, bx)=
ax-nbx=q    raccogliendo x
x(a-nb)=q   a-nb è il risultato di mod(a,b)
mod(a,b)=q1
x*q1=q
mod(ax, bx)=x*mod(a,b)
```
Ovvero restituisce x oppure un numero nella forma yx

Dopo è calcolata la media. media(x+e1, x+e2, x+e3) = x+media(e1+e2+e3), ovvero x più la media degli errori.


## Makefile
All'uscita dal ciclo che invia SIGINT al supervisor, ho inserito sleep 0.5 per evitare che i due segnali SIGINT siano inviati troppo velocemente (il supervisor potrebbe ricevere un segnale solo)


## Misura
Il primo ciclo legge tutti i file dati in input e seleziona solo le righe contenenti il secret del client. Per ogni coppia id-secret , è estratto l'ultimo secret stampato dal supervisor (la stima più aggiornata). Il valore assoluto della differenza fra i due è inserito in un array. Ho usato la command substitution perché bash esegue in una sottoshell il ciclo while dopo la pipe. Un altra possibile soluzione è la process subtitution, che però non è conforme POSIX


## Net
POSIX non definisce nulla riguardo l'endianness, e le funzioni di conversione da e verso network byte order di uint64_t fornite in endian.h non sono conformi POSIX. Le funzioni che ho scritto eseguono un controllo a runtime.

Lo scambio di bit è implementato usando le funzioni di byteorder e una union che contiene un uint64 o un array di due uint32. Altre possibili soluzioni sono usare puntatori a char o uint32, oppure una macro con bitwise operators.

Ho preferito la prima soluzione perché più leggibile. 

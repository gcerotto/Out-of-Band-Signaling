#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/types.h>
#include "header.h"
#include "net.h"

#define TR_EC_MAL(mal) \
if (!(mal)) {\
  fprintf(stderr, "error in %s line %d : out of memory\n",  __func__ , __LINE__);\
  PTHREAD_EXIT\
}

#define PTHREAD_EXIT \
close(c_skt);\
free(arg);\
free(array);\
pthread_exit(NULL);

#define SOCK_PRE "OOB-server-"
#define ARRAY_LEN 8
#define MAXERR 10

char *sock;

void cleanup() {
  if(sock)
    unlink(sock);
}

void gestore (int sig) {
  cleanup();
  _exit(0);
}

int guess_sec(int *diffarr, int index) {
  int i, min, res=0;
  char iter=true;
  
  while(iter) {
    iter=false;
    min=diffarr[0];
    for(i=1; i<index; i++) {
      if (diffarr[i] < min)
	min=diffarr[i];
    }
    
    for(i=0; i<index; i++) {
      diffarr[i]%=min;
      if(diffarr[i] > MAXERR)
	iter=true;
      else
	diffarr[i]+=min;
    }
  }
  
  for(i=0; i<index; i++) {
    res+=diffarr[i];
  }
  res/=index;
  
  return res;
}

void * worker(void *arg) {
  int i, len, array_len=ARRAY_LEN, index=0;
  int secret;
  int c_skt=((struct info *)arg)->c_skt;
  int id=((struct info *)arg)->id;
  int pipewrite=((struct info *)arg)->pipewrite;
  uint64_t cid;
  struct timespec tp;
  struct timespec *array;
  int *diffarr;
  struct result res;
  
  if (!(array=malloc(array_len*sizeof(struct timespec)))) {
    close(c_skt);
    free(arg);
    fprintf(stderr, "error: out of memory\n");
    pthread_exit(NULL);
  }
  
  while ((len=read(c_skt, &cid, sizeof(uint64_t))) != 0 ) {
    clock_gettime(CLOCK_MONOTONIC, &tp);
    cid=my_be64toh(cid);
    
    if (len==-1) {
      perror("error: read");
      PTHREAD_EXIT;
    }
    if (array_len == index ) {
      array_len=array_len*2;
      TR_EC_MAL(array=realloc(array, array_len*sizeof(struct timespec)))
    }
    array[index++]=tp;
    printf("SERVER %d INCOMING FROM %"PRIX64" @ %u.%lu\n", id, cid, (unsigned int)tp.tv_sec, tp.tv_nsec);
  }
  
  if (index<2) {
    PTHREAD_EXIT;
  }
  index--;
  TR_EC_MAL(diffarr=malloc((index)*sizeof(int)))
  
  for(i=0; i<index; i++)
    diffarr[i]=(int) (array[i+1].tv_sec-array[i].tv_sec) * 1000 + 
				(array[i+1].tv_nsec-array[i].tv_nsec)/1000000 ;
  
  secret=guess_sec(diffarr, index);
    
  printf("SERVER %d CLOSING %"PRIX64" ESTIMATE %d\n", id, cid, secret);
  
  res.id=id;
  res.cid=cid;
  res.secret=secret;
  
  EC_NEG1(write(pipewrite, &res, sizeof(struct result)));
  
  free(diffarr);
  PTHREAD_EXIT;
}

void server(int id, int pipefd) {
  char *buf, *num;
  int s_skt;
  pthread_t tid;
  struct sockaddr_un sa;
  struct info *inf;
  sigset_t set;
  struct sigaction s;
  
  sigemptyset(&set);
  sigaddset(&set, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  memset(&s, 0, sizeof(struct sigaction));
  s.sa_handler=gestore;
  sigaction(SIGTERM, &s, NULL);
  if (atexit(cleanup) != 0) {
    fprintf(stderr, "error: server cannot set exit function\n");
    exit(EXIT_FAILURE);
  }

  EC_MAL(buf=malloc(UNIX_PATH_MAX));
  memset(&sa, 0, sizeof(struct sockaddr_un));
  
  strcpy(buf, SOCK_PRE);
  num=buf+strlen(SOCK_PRE);
  sprintf(num, "%d", id);
  
  sa.sun_family=AF_UNIX;
  strncpy(sa.sun_path, buf, UNIX_PATH_MAX);
  free(buf);
  sock=sa.sun_path;
  EC_NEG1(s_skt=socket(AF_UNIX, SOCK_STREAM, 0));
  EC_NEG1(bind(s_skt, (struct sockaddr *)&sa, sizeof(sa)));
  EC_NEG1(listen(s_skt,SOMAXCONN));
  
  printf("SERVER %d ACTIVE\n", id);
  
  while(true) {
    EC_MAL(inf=malloc(sizeof(struct info)));
    inf->id=id;
    inf->pipewrite=pipefd;
    EC_NEG1(inf->c_skt=accept(s_skt, NULL, 0));
    printf("SERVER %d CONNECT FROM CLIENT\n", id);
    
    if (pthread_create(&tid, NULL, worker, (void *) inf) !=0 ) {
      fprintf(stderr, "error: server cannot create thread\n");
      exit(EXIT_FAILURE);
    }
    pthread_detach(tid);
  }
}
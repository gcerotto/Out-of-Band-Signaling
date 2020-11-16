#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "header.h"
#include "net.h"

#define SOCK_PRE "OOB-server-"


static void print_help_quit(char *name) {
  printf("Usage: %s p k w\nConnect to p of k server aviable and send w messagges. 1<=p<=k and w>3p\n", name);
  exit(EXIT_FAILURE);
}

uint64_t get_rand64() {
  union bit64 val;
  
  val.thir[0]=(uint32_t)random();
  val.thir[1]=(uint32_t)random();
    
  return val.six;
}

int exists(int num, int *array, int dim) {
  int i;
  
  if (dim==0) return 0;
  
  for(i=0; i<dim; i++) {
    if (array[i]==num) return 1;
  }
  
  return 0;
}

void fill_array(int *servers, int num_conn, int num_server) {
  int i, test;
  
  for(i=0; i<num_conn; i++) {
    do
      test=(random()%num_server)+1;
    while(exists(test, servers, i));
    
    servers[i]=test; 
  }
}

void connect_array(int *servers, int *arrayfd, struct sockaddr_un *sa, int len) {
  int i;
  char *buf, *num;
  
  EC_MAL(buf=malloc(UNIX_PATH_MAX));
  
  strcpy(buf, SOCK_PRE);
  num=buf+strlen(SOCK_PRE);
  
  for(i=0; i<len; i++) {
    sprintf(num, "%d", servers[i]);
    printf("ciclo %d server %d\n", i, servers[i]);
    memset(sa+i, 0, sizeof(struct sockaddr_un));
    sa[i].sun_family=AF_UNIX;
    strncpy(sa[i].sun_path, buf, UNIX_PATH_MAX);
    EC_NEG1(arrayfd[i]=socket(AF_UNIX, SOCK_STREAM, 0));
    
    if (connect(arrayfd[i],(struct sockaddr*)(sa+i), sizeof(struct sockaddr_un)) == -1) {
      fprintf(stderr, "Could not connect to server %d\n", servers[i]);
      perror("connect");
      exit(EXIT_FAILURE);
    }
  }
  free(buf);
}


int main(int argc, char *argv[]) {
  int num_server, num_conn, num_mess, secret, i, ind;
  uint64_t id, netid;
  int *servers, *arrayfd;
  struct timespec tp;
  struct sockaddr_un *sa;
  
  if (argc != 4)
    print_help_quit(argv[0]);
  
  num_conn=atoi(argv[1]);
  num_server=atoi(argv[2]);
  num_mess=atoi(argv[3]);
  
  if(num_conn<1 || num_conn>num_server)
    print_help_quit(argv[0]);
  
  if (num_mess<(3*num_conn))
    print_help_quit(argv[0]);
  
  EC_NEG1(clock_gettime(CLOCK_REALTIME, &tp));
  srandom((unsigned int)tp.tv_nsec);
  secret=(random()%3000)+1;
  
  tp.tv_sec=(secret/1000);
  tp.tv_nsec=(secret%1000)*1000000;
  
  id=get_rand64();
  netid=my_htobe64(id);
  
  printf("CLIENT %"PRIX64" SECRET %d\n", id, secret);
  
  EC_MAL(servers=malloc(num_conn*sizeof(int)));
  EC_MAL(arrayfd=malloc(num_conn*sizeof(int)));
  EC_MAL(sa=malloc(num_conn*sizeof(struct sockaddr_un)));
  
  fill_array(servers, num_conn, num_server);
  
  connect_array(servers, arrayfd, sa, num_conn);
  
  for(i=0; i<num_mess; i++) {
    ind=(random()%num_conn);
    EC_NEG1(write(arrayfd[ind], &netid, 8));
    EC_NEG1(nanosleep(&tp, NULL));
  }
  
  for(i=0; i<num_conn; i++)
    close(arrayfd[i]);
  
  printf("CLIENT %"PRIX64" DONE\n", id);
  
  exit(EXIT_SUCCESS);
}

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

#define SOCK_PRE "OOB-server-"
#define ARRAY_LEN 8


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_help_quit(char *name) {
  printf("Usage: %s number-of-servers-to-start\n", name);
  exit(EXIT_FAILURE);
}

int compar(const void *p, const void *q) {
  int *a=(int *)p;
  int *b=(int *)q;
  
  if (*a==*b)
    return 0;
  else if (*a < *b)
    return -1;
  else
    return 1;
}

int mode(int *array, int len) {
  int i;
  int res=0, res_count=0;
  int cur, cur_count=0;
  
  qsort(array, len, sizeof(int), compar);
  
  cur=array[0];
  for(i=0; i<len; i++) {
    if (array[i]!=cur) {
      if (cur_count > res_count) {
	res=cur;
	res_count=cur_count;
      }
      cur=array[i];
      cur_count=1;
    }
    else
      cur_count++;
  }
  if (cur_count > res_count)
    return cur;
  else
    return res;
}

void kill_servers(int *pids, int num_server) {
  int i;
  
  if(pids) {
    for(i=0; i<num_server; i++) {
      kill(pids[i], SIGTERM);
    }
  }
}

void * sighandler(void *arg) {
  struct pass *pa=(struct pass *)arg;
  sigset_t set;
  int sig;
  struct timespec new, old;
  clients *slide;
  int secret;
  
  old.tv_sec=0;
  old.tv_nsec=0;
  
  sigemptyset(&set);
  sigaddset(&set, SIGINT);

  while(!sigwait(&set, &sig)) {
    clock_gettime(CLOCK_MONOTONIC, &new);
    
    if( ( (new.tv_sec * 1000) - (old.tv_sec * 1000) ) + 
      ( (new.tv_nsec / 1000000) - (old.tv_nsec / 1000000) ) < 1000) {
	printf("SUPERVISOR EXITING\n");
	kill_servers(pa->pids, pa->num_server);
	exit(EXIT_SUCCESS);
    }
    
    pthread_mutex_lock(&mutex);
    slide=*(pa->clist);
    while(slide) {
      secret=mode(slide->secrets, slide->num_el);
      printf("SUPERVISOR ESTIMATE %d FOR %"PRIX64" BASED ON %d\n", secret, slide->cid, slide->num_el);
      slide=slide->next;
    }
    pthread_mutex_unlock(&mutex);

    old=new;
  }
  
  return NULL;
}

void add_val(clients *clist, struct result res, int num_server) {
  if (!clist->secrets) {
    clist->secrets=malloc(num_server*sizeof(int));
    clist->num_el=0;
  }
  clist->secrets[clist->num_el]=res.secret;
  clist->num_el++;
}

void add_res(clients **clist, struct result res, int num_server) {
  clients *new, *slide, *prec;
  char insert=false;
  
  pthread_mutex_lock(&mutex);
  slide=*clist;
  while(slide && !insert) {
    if (slide->cid==res.cid) {
      add_val(slide, res, num_server);
      insert=true;
    }
    prec=slide;
    slide=slide->next;
  }
  if (!insert) {
    EC_MAL(new=malloc(sizeof(clients)));
    new->cid=res.cid;
    new->secrets=NULL;
    new->next=NULL;
    if (!*clist)
      *clist=new;
    else
      prec->next=new;
    add_val(new, res, num_server);
  }
  pthread_mutex_unlock(&mutex);
}


int main(int argc, char *argv[]) {
  int num_server, i, len;
  pid_t *pids;
  int pipefd[2];
  pthread_t tid;
  sigset_t set;
  struct result res;
  clients **clist;
  struct pass pa;
  
  setvbuf(stdout, NULL, _IOLBF, 0);
  
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  
  if (argc != 2)
    print_help_quit(argv[0]);
  
  if(strcmp(argv[1], "--help")==0)
    print_help_quit(argv[0]);
  
  num_server=atoi(argv[1]);
  
  EC_MAL(clist=malloc(sizeof(clients *)));
  *clist=NULL;
  
  printf("SUPERVISOR STARTING %d\n", num_server);
  
  EC_MAL(pids=malloc(num_server*sizeof(pid_t)));
  
  EC_NEG1(pipe(pipefd));
  EC_NEG1(fcntl(pipefd[0], F_SETFD, FD_CLOEXEC));
  
  for(i=0; i<num_server; i++) {
    EC_NEG1(pids[i]=fork());
    if (pids[i]==0) {
      server(i+1, pipefd[1]); /* does not return */
    }
  }
  close(pipefd[1]);
  
  pa.pids=pids;
  pa.clist=clist;
  pa.num_server=num_server;
  if (pthread_create(&tid, NULL, sighandler, (void *)&pa) !=0 ) {
    fprintf(stderr, "error: supervisor cannot create thread\n");
    exit(EXIT_FAILURE);
  }
  
  while ((len=read(pipefd[0], &res, sizeof(struct result))) != 0 ) {
    if (len==-1) {
      perror("error: supervisor, read from pipe");
      kill_servers(pids, num_server);
      exit(EXIT_FAILURE);
    }
    add_res(clist, res, num_server);
    printf("SUPERVISOR ESTIMATE %d FOR %"PRIX64" FROM %d\n", res.secret, res.cid, res.id);
  }
  
  exit(EXIT_SUCCESS);
}

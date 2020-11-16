#define UNIX_PATH_MAX 108

#define EC_NEG1(fun) \
if ( (fun) == -1 ) {\
  fprintf(stderr, "error in %s line %d, ", __func__ , __LINE__);\
  perror(#fun);\
  exit(EXIT_FAILURE);\
}

#define EC_MAL(mal) \
if (!(mal)) {\
  fprintf(stderr, "error in %s line %d : out of memory\n",  __func__ , __LINE__);\
  exit(EXIT_FAILURE);\
}

#define true 1
#define false 0

struct info {
  int c_skt;
  int id;
  int pipewrite;
};

struct result {
  int id;
  uint64_t cid;
  int secret;
};

struct pass {
  pid_t *pids;
  int num_server;
  struct clients_ **clist;
};

typedef struct clients_ {
  uint64_t cid;
  int *secrets;
  int num_el;
  struct clients_ *next;
} clients;

void server(int, int);
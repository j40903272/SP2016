#ifndef _CSIEBOX_SERVER_
#define _CSIEBOX_SERVER_

#ifdef __cplusplus
extern "C" {
#endif

#include "csiebox_common.h"
#include "threadpool.h"

#include <limits.h>

typedef struct {
    void (*function)(void *);
    void *argument;
} threadpool_task_t;

struct threadpool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  threadpool_task_t *queue;
  int thread_count;
  int queue_size;
  int head;
  int tail;
  int count;
  int shutdown;
  int started;
};


typedef struct {
  char user[USER_LEN_MAX];
  char passwd_hash[MD5_DIGEST_LENGTH];
} csiebox_account_info;

typedef struct {
  csiebox_account_info account;
  int conn_fd;
} csiebox_client_info;


typedef struct {
  struct {
    char path[PATH_MAX];
    char account_path[PATH_MAX];
    int thread_num;
    char run_path[PATH_MAX];
    char fifo_path[PATH_MAX];
  } arg;
  int listen_fd;
  int client_num;
  char pid[32];
  csiebox_client_info** client;
  pthread_t* threads;
  threadpool_t *pool;
} csiebox_server;

typedef struct {
  csiebox_server* server;
  int conn_fd;
  char complete;
} req_arg;

void csiebox_server_init(csiebox_server** server, int argc, char** argv);
int csiebox_server_run(csiebox_server* server);
void csiebox_server_destroy(csiebox_server** server);
void daemonize(const char *cmd, csiebox_server* server);

#ifdef __cplusplus
}
#endif

#endif

#ifndef CHTTP_TPOOL_H
#define CHTTP_TPOOL_H

#include <pthread.h>

#define IP_BUFFER 46 // handle v4 and v6

typedef struct chttp_task {
  int client_fd;
  char client_ip[IP_BUFFER];
  struct chttp_task *next;
} chttp_task_t;

typedef struct {
  chttp_task_t *head;
  chttp_task_t *tail;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int shutdown;
} chttp_queue_t;

typedef struct {
  pthread_t *threads;
  int thread_count;
  chttp_queue_t queue;
  const char *public_dir;
} chttp_tpool_t;

chttp_tpool_t *chttp_tpool_create(int thread_count, const char *public_dir);
int chttp_tpool_push(chttp_tpool_t *pool, int client_fd, const char *client_ip);
void chttp_tpool_destroy(chttp_tpool_t *pool);

#endif
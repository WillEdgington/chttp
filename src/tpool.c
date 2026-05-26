#include "chttp/tpool.h"
#include "chttp/server.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *worker_routine(void *arg) {
  chttp_tpool_t *pool = (chttp_tpool_t *)arg;
  chttp_queue_t *queue = &pool->queue;

  while (1) {
    pthread_mutex_lock(&queue->lock);

    while (queue->head == NULL && queue->shutdown == 0) {
      pthread_cond_wait(&queue->cond, &queue->lock);
    }

    if (queue->shutdown == 1 && queue->head == NULL) {
      pthread_mutex_unlock(&queue->lock);
      break;
    }

    chttp_task_t *task = queue->head;
    queue->head = task->next;
    if (queue->head == NULL)
      queue->tail = NULL;

    pthread_mutex_unlock(&queue->lock);
    chttp_handle_connection(task->client_fd, pool->public_dir, task->client_ip);
    close(task->client_fd);
    free(task);
  }

  return NULL;
}

chttp_tpool_t *chttp_tpool_create(int thread_count, const char *public_dir) {
  chttp_tpool_t *pool = malloc(sizeof(chttp_tpool_t));
  if (pool == NULL)
    return NULL;

  pool->thread_count = thread_count;
  pool->public_dir = public_dir;
  pool->queue.head = NULL;
  pool->queue.tail = NULL;
  pool->queue.shutdown = 0;

  if (pthread_mutex_init(&pool->queue.lock, NULL) != 0 ||
      pthread_cond_init(&pool->queue.cond, NULL) != 0) {
    free(pool);
    return NULL;
  }

  pool->threads = malloc(sizeof(pthread_t) * thread_count);
  if (pool->threads == NULL) {
    pthread_mutex_destroy(&pool->queue.lock);
    pthread_cond_destroy(&pool->queue.cond);
    free(pool);
    return NULL;
  }

  for (int i = 0; i < thread_count; i++) {
    if (pthread_create(&pool->threads[i], NULL, worker_routine, pool) != 0) {
      chttp_tpool_destroy(pool);
      return NULL;
    }
  }
  return pool;
}

int chttp_tpool_push(chttp_tpool_t *pool, int client_fd,
                     const char *client_ip) {
  chttp_task_t *task = malloc(sizeof(chttp_task_t));
  if (task == NULL)
    return -1;

  task->client_fd = client_fd;
  strncpy(task->client_ip, client_ip, IP_BUFFER - 1);
  task->client_ip[IP_BUFFER - 1] = '\0';
  task->next = NULL;

  pthread_mutex_lock(&pool->queue.lock);
  if (pool->queue.shutdown) {
    pthread_mutex_unlock(&pool->queue.lock);
    free(task);
    return -1;
  }

  if (pool->queue.head == NULL) {
    pool->queue.head = task;
    pool->queue.tail = task;
  } else {
    pool->queue.tail->next = task;
    pool->queue.tail = task;
  }

  pthread_cond_signal(&pool->queue.cond);
  pthread_mutex_unlock(&pool->queue.lock);
  return 0;
}

void chttp_tpool_destroy(chttp_tpool_t *pool) {
  if (pool == NULL)
    return;

  pthread_mutex_lock(&pool->queue.lock);
  pool->queue.shutdown = 1;
  pthread_cond_broadcast(&pool->queue.cond);
  pthread_mutex_unlock(&pool->queue.lock);

  for (int i = 0; i < pool->thread_count; i++)
    pthread_join(pool->threads[i], NULL);

  chttp_task_t *cur = pool->queue.head;
  while (cur != NULL) {
    chttp_task_t *next = cur->next;
    close(cur->client_fd);
    free(cur);
    cur = next;
  }

  pthread_mutex_destroy(&pool->queue.lock);
  pthread_cond_destroy(&pool->queue.cond);
  free(pool->threads);
  free(pool);
}
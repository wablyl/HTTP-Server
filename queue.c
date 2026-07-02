#include "queue.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

struct queue {
  void **buffer;
  int capacity;
  int count;
  int head;
  int tail;
  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
};

queue_t *queue_new(int size) {
  queue_t *q = (queue_t *)malloc(sizeof(queue_t));
  if (!q)
    return NULL;
  q->buffer = (void **)malloc(sizeof(void *) * size);
  if (!q->buffer) {
    free(q);
    return NULL;
  }
  q->capacity = size;
  q->count = 0;
  q->head = 0;
  q->tail = 0;
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->not_full, NULL);
  pthread_cond_init(&q->not_empty, NULL);
  return q;
}

void queue_delete(queue_t **q) {
  if (q == NULL || *q == NULL)
    return;
  pthread_mutex_destroy(&(*q)->lock);
  pthread_cond_destroy(&(*q)->not_full);
  pthread_cond_destroy(&(*q)->not_empty);
  free((*q)->buffer);
  free(*q);
  *q = NULL;
}

bool queue_push(queue_t *q, void *elem) {
  if (q == NULL)
    return false;
  pthread_mutex_lock(&q->lock);
  while (q->count == q->capacity) {
    pthread_cond_wait(&q->not_full, &q->lock);
  }
  q->buffer[q->tail] = elem;
  q->tail = (q->tail + 1) % q->capacity;
  q->count++;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
  return true;
}

bool queue_pop(queue_t *q, void **elem) {
  if (q == NULL || elem == NULL)
    return false;
  pthread_mutex_lock(&q->lock);
  while (q->count == 0) {
    pthread_cond_wait(&q->not_empty, &q->lock);
  }
  *elem = q->buffer[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->count--;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->lock);
  return true;
}

#include "rwlock.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

struct rwlock {
  PRIORITY priority;
  uint32_t n;
  int active_readers;
  int active_writers;
  int waiting_readers;
  int waiting_writers;
  uint32_t n_way_count;
  pthread_mutex_t lock;
  pthread_cond_t readers_cv;
  pthread_cond_t writers_cv;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
  rwlock_t *rw = (rwlock_t *)malloc(sizeof(rwlock_t));
  if (!rw)
    return NULL;
  rw->priority = p;
  rw->n = n;
  rw->active_readers = 0;
  rw->active_writers = 0;
  rw->waiting_readers = 0;
  rw->waiting_writers = 0;
  rw->n_way_count = 0;
  pthread_mutex_init(&rw->lock, NULL);
  pthread_cond_init(&rw->readers_cv, NULL);
  pthread_cond_init(&rw->writers_cv, NULL);
  return rw;
}

void rwlock_delete(rwlock_t **l) {
  if (l == NULL || *l == NULL)
    return;
  pthread_mutex_destroy(&(*l)->lock);
  pthread_cond_destroy(&(*l)->readers_cv);
  pthread_cond_destroy(&(*l)->writers_cv);
  free(*l);
  *l = NULL;
}

void reader_lock(rwlock_t *rw) {
  pthread_mutex_lock(&rw->lock);
  rw->waiting_readers++;
  while (rw->active_writers > 0 ||
         (rw->priority == WRITERS && rw->waiting_writers > 0) ||
         (rw->priority == N_WAY && rw->waiting_writers > 0 &&
          rw->n_way_count >= rw->n)) {
    pthread_cond_wait(&rw->readers_cv, &rw->lock);
  }
  rw->waiting_readers--;
  rw->active_readers++;
  rw->n_way_count++;
  pthread_mutex_unlock(&rw->lock);
}

void reader_unlock(rwlock_t *rw) {
  pthread_mutex_lock(&rw->lock);
  rw->active_readers--;
  if (rw->active_readers == 0)
    pthread_cond_signal(&rw->writers_cv);
  pthread_mutex_unlock(&rw->lock);
}

void writer_lock(rwlock_t *rw) {
  pthread_mutex_lock(&rw->lock);
  rw->waiting_writers++;
  while (rw->active_readers > 0 || rw->active_writers > 0 ||
         (rw->priority == READERS && rw->waiting_readers > 0) ||
         (rw->priority == N_WAY && rw->waiting_readers > 0 &&
          rw->n_way_count < rw->n)) {
    pthread_cond_wait(&rw->writers_cv, &rw->lock);
  }
  rw->waiting_writers--;
  rw->active_writers = 1;
  rw->n_way_count = 0;
  pthread_mutex_unlock(&rw->lock);
}

void writer_unlock(rwlock_t *rw) {
  pthread_mutex_lock(&rw->lock);
  rw->active_writers = 0;
  if (rw->priority == READERS && rw->waiting_readers > 0) {
    pthread_cond_broadcast(&rw->readers_cv);
  } else if (rw->priority == WRITERS && rw->waiting_writers > 0) {
    pthread_cond_signal(&rw->writers_cv);
  } else if (rw->priority == N_WAY) {
    if (rw->waiting_readers > 0 && rw->n_way_count < rw->n)
      pthread_cond_broadcast(&rw->readers_cv);
    else if (rw->waiting_writers > 0)
      pthread_cond_signal(&rw->writers_cv);
    else
      pthread_cond_broadcast(&rw->readers_cv);
  } else {
    if (rw->waiting_readers > 0)
      pthread_cond_broadcast(&rw->readers_cv);
    else
      pthread_cond_signal(&rw->writers_cv);
  }
  pthread_mutex_unlock(&rw->lock);
}

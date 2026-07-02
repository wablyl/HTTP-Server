#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#include <stdint.h>

typedef enum { READERS, WRITERS, N_WAY } PRIORITY;
typedef struct rwlock rwlock_t;

rwlock_t *rwlock_new(PRIORITY p, uint32_t n);
void rwlock_delete(rwlock_t **l);
void reader_lock(rwlock_t *rw);
void reader_unlock(rwlock_t *rw);
void writer_lock(rwlock_t *rw);
void writer_unlock(rwlock_t *rw);

#endif

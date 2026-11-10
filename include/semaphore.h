#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H
#include <bits/features.h>
#include <fcntl.h>
#include <time.h>
#define __NEED_clockid_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

/* the size of glibc's */
typedef struct {
	volatile int __val[4];
	long __pad[2];
} sem_t;

#define SEM_FAILED ((sem_t *)0)

__BEGIN_DECLS
int sem_clockwait(sem_t *__RESTRICT, clockid_t, const struct timespec *__RESTRICT);
int sem_close(sem_t *);
int sem_destroy(sem_t *);
int sem_getvalue(sem_t *__RESTRICT, int *__RESTRICT);
int sem_init(sem_t *, int, unsigned);
sem_t *sem_open(const char *, int, ...);
int sem_post(sem_t *);
int sem_timedwait(sem_t *__RESTRICT, const struct timespec *__RESTRICT);
int sem_trywait(sem_t *);
int sem_unlink(const char *);
int sem_wait(sem_t *);
__END_DECLS

#endif

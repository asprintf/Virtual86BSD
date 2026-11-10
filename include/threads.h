#ifndef _THREADS_H
#define _THREADS_H
#include <bits/features.h>
#include <time.h>

#if !defined(__cplusplus) && !(defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L)
#define thread_local _Thread_local
#endif

#define ONCE_FLAG_INIT 0
#define TSS_DTOR_ITERATIONS 4

/* thrd_t is pthread_t; mtx_t and cnd_t are laid out as pthread_mutex_t
 * and pthread_cond_t */
typedef struct __pthread *thrd_t;
typedef int once_flag;
typedef unsigned tss_t;
typedef union { int __i[10]; volatile int __vi[10]; void *__p[5]; } mtx_t;
typedef union { int __i[12]; volatile int __vi[12]; void *__p[6]; } cnd_t;
typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

/* the values are glibc's */
enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };
enum { thrd_success = 0, thrd_busy = 1, thrd_error = 2, thrd_nomem = 3, thrd_timedout = 4 };

__BEGIN_DECLS
void call_once(once_flag *, void (*)(void));
int cnd_broadcast(cnd_t *);
void cnd_destroy(cnd_t *);
int cnd_init(cnd_t *);
int cnd_signal(cnd_t *);
int cnd_timedwait(cnd_t *__RESTRICT, mtx_t *__RESTRICT, const struct timespec *__RESTRICT);
int cnd_wait(cnd_t *, mtx_t *);
void mtx_destroy(mtx_t *);
int mtx_init(mtx_t *, int);
int mtx_lock(mtx_t *);
int mtx_timedlock(mtx_t *__RESTRICT, const struct timespec *__RESTRICT);
int mtx_trylock(mtx_t *);
int mtx_unlock(mtx_t *);
int thrd_create(thrd_t *, thrd_start_t, void *);
thrd_t thrd_current(void);
int thrd_detach(thrd_t);
int thrd_equal(thrd_t, thrd_t);
__NORETURN void thrd_exit(int);
int thrd_join(thrd_t, int *);
int thrd_sleep(const struct timespec *, struct timespec *);
void thrd_yield(void);
int tss_create(tss_t *, tss_dtor_t);
void tss_delete(tss_t);
void *tss_get(tss_t);
int tss_set(tss_t, void *);
__END_DECLS

#endif

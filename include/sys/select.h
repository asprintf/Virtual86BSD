#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <bits/features.h>
#define __NEED_time_t
#define __NEED_suseconds_t
#define __NEED_sigset_t
#define __NEED_struct_timespec
#define __NEED_struct_timeval
#include <bits/alltypes.h>

#define FD_SETSIZE 1024

typedef struct {
	unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

#define __FD_WORD(d) ((d) / (8 * sizeof(long)))
#define __FD_BIT(d)  (1UL << ((d) % (8 * sizeof(long))))
#define FD_ZERO(s)  do { int __i; for (__i = 0; __i < (int)(FD_SETSIZE / (8 * sizeof(long))); __i++) \
                         (s)->fds_bits[__i] = 0; } while (0)
#define FD_SET(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] |= __FD_BIT(d)))
#define FD_CLR(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] &= ~__FD_BIT(d)))
#define FD_ISSET(d, s) (!!((s)->fds_bits[__FD_WORD(d)] & __FD_BIT(d)))

__BEGIN_DECLS
int pselect(int, fd_set *__RESTRICT, fd_set *__RESTRICT, fd_set *__RESTRICT,
            const struct timespec *__RESTRICT, const sigset_t *__RESTRICT);
int select(int, fd_set *__RESTRICT, fd_set *__RESTRICT, fd_set *__RESTRICT,
           struct timeval *__RESTRICT);
__END_DECLS
#endif

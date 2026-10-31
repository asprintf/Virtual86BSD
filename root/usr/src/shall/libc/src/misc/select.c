#include <sys/select.h>
#include <poll.h>
#include <errno.h>
#include "syscall.h"

int ppoll(struct pollfd fds[], nfds_t n, const struct timespec *__RESTRICT ts,
          const sigset_t *__RESTRICT mask)
{
	struct timespec t;
	if (ts) t = *ts;  /* the kernel updates its argument */
	return (int)__sys_cp5(SYS_ppoll, fds, n, ts ? &t : 0, mask, 8);
}

int poll(struct pollfd fds[], nfds_t n, int ms)
{
	struct timespec t;
	if (ms >= 0) { t.tv_sec = ms / 1000; t.tv_nsec = ms % 1000 * 1000000L; }
	return (int)__sys_cp5(SYS_ppoll, fds, n, ms >= 0 ? &t : 0, 0, 8);
}

int pselect(int n, fd_set *__RESTRICT r, fd_set *__RESTRICT w, fd_set *__RESTRICT e,
            const struct timespec *__RESTRICT ts, const sigset_t *__RESTRICT mask)
{
	struct timespec t;
	struct { const sigset_t *m; unsigned long size; } data = { mask, 8 };
	if (ts) {
		if (ts->tv_sec < 0 || (unsigned long)ts->tv_nsec >= 1000000000UL) { errno = EINVAL; return -1; }
		t = *ts;
	}
	return (int)__sys_cp6(SYS_pselect6, n, r, w, e, ts ? &t : 0, &data);
}

int select(int n, fd_set *__RESTRICT r, fd_set *__RESTRICT w, fd_set *__RESTRICT e,
           struct timeval *__RESTRICT tv)
{
	struct timespec t;
	if (tv) {
		if (tv->tv_sec < 0 || tv->tv_usec < 0) { errno = EINVAL; return -1; }
		t.tv_sec = tv->tv_sec + tv->tv_usec / 1000000;
		t.tv_nsec = tv->tv_usec % 1000000 * 1000;
	}
	return pselect(n, r, w, e, tv ? &t : 0, 0);
}

/* Futex waits and wakes.  An absolute timeout is measured against
 * CLOCK_REALTIME or CLOCK_MONOTONIC by the kernel (FUTEX_WAIT_BITSET). */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_WAIT_BITSET 9
#define FUTEX_PRIVATE 128
#define FUTEX_CLOCK_REALTIME 256

static int fwait(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv, int cp)
{
	long r;
	int op = FUTEX_WAIT;
	if (at) {
		if ((unsigned long)at->tv_nsec >= 1000000000UL)
			return EINVAL;
		if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC)
			return EINVAL;
		if (at->tv_sec < 0)
			return ETIMEDOUT;
		op = FUTEX_WAIT_BITSET | (clk == CLOCK_REALTIME ? FUTEX_CLOCK_REALTIME : 0);
	}
	if (priv)
		op |= FUTEX_PRIVATE;
	if (cp)
		r = __syscall_cp6(SYS_futex, addr, op, val, at, 0, at ? -1 : 0);
	else
		r = __syscall6x(SYS_futex, addr, op, val, at, 0, at ? -1 : 0);
	if (r == -ETIMEDOUT || r == -EINTR || r == -ECANCELED || r == -EINVAL)
		return (int)-r;
	return 0;
}

int __timedwait(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv)
{
	return fwait(addr, val, clk, at, priv, 0);
}

int __timedwait_cp(volatile int *addr, int val, clockid_t clk, const struct timespec *at, int priv)
{
	return fwait(addr, val, clk, at, priv, 1);
}

void __wait(volatile int *addr, int val, int priv)
{
	fwait(addr, val, 0, 0, priv, 0);
}

void __wake(volatile int *addr, int cnt, int priv)
{
	if (cnt < 0)
		cnt = 0x7fffffff;
	__syscall3(SYS_futex, addr, FUTEX_WAKE | (priv ? FUTEX_PRIVATE : 0), cnt);
}

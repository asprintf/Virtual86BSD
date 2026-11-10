/* Internal futex lock (Drepper's "mutex 2": 0 free, 1 locked, 2 contended). */
#include "lock.h"
#include "atomic.h"
#include "syscall.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE 128

void __futex_wait(volatile int *addr, int val)
{
	__syscall4(SYS_futex, addr, FUTEX_WAIT | FUTEX_PRIVATE, val, 0);
}

void __futex_wake(volatile int *addr, int cnt)
{
	if (cnt < 0) cnt = 0x7fffffff;
	__syscall3(SYS_futex, addr, FUTEX_WAKE | FUTEX_PRIVATE, cnt);
}

void __lock(volatile int *l)
{
	int spins;
	if (!a_cas(l, 0, 1)) return;
	for (spins = 0; spins < 100; spins++) {
		if (!*l && !a_cas(l, 0, 1)) return;
		a_spin();
	}
	while (a_swap(l, 2)) __futex_wait(l, 2);
}

void __unlock(volatile int *l)
{
	if (a_swap(l, 0) == 2) __futex_wake(l, 1);
}

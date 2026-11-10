/* The lock of the thread list, and blocking all signals. */
#include "pthread_impl.h"
#include "lock.h"
#include "syscall.h"

volatile int __thread_list_lock;

void __tl_lock(void) { LOCK(&__thread_list_lock); }
void __tl_unlock(void) { UNLOCK(&__thread_list_lock); }

void __block_all_sigs(unsigned long *old)
{
	unsigned long all = -1UL;
	__syscall4(SYS_rt_sigprocmask, 0 /* SIG_BLOCK */, &all, old, 8);
}

void __restore_sigs(const unsigned long *old)
{
	__syscall4(SYS_rt_sigprocmask, 2 /* SIG_SETMASK */, old, 0, 8);
}

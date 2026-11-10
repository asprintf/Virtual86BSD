#include <unistd.h>
#include <signal.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "clofork.h"
#include "libc.h"

void __malloc_atfork(int);
void (*__atfork_hook)(int);  /* set by pthread_atfork(): -1 prepare, 0 parent, 1 child */

/* _Fork: async-signal-safe; no handlers, no locking. */
pid_t _Fork(void)
{
	long r;
	unsigned long all = -1UL, old;
	/* block signals so that no handler runs in the child with stale state */
	__syscall4(SYS_rt_sigprocmask, SIG_BLOCK, &all, &old, 8);
	r = __syscall5(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (r == 0) {
		/* the child: the only thread */
		struct __pthread *self = __pthread_self();
		self->tid = (int)__syscall0(SYS_gettid);
		self->next = self->prev = self;
		self->robust_registered = 0;
		self->robust_list.head = &self->robust_list.head;
		self->robust_list.pending = 0;
		__libc.threads_minus_1 = 0;
		__thread_list_lock = 0;
		__clofork_close_all();
	}
	__syscall4(SYS_rt_sigprocmask, SIG_SETMASK, &old, 0, 8);
	return (pid_t)__syscall_ret((unsigned long)r);
}

pid_t fork(void)
{
	pid_t r;
	if (__atfork_hook) __atfork_hook(-1);
	__tl_lock();
	__malloc_atfork(-1);
	r = _Fork();
	__malloc_atfork(r == 0 ? 1 : 0);
	if (r) __tl_unlock();       /* the child's is reset */
	if (__atfork_hook) __atfork_hook(r == 0 ? 1 : 0);
	return r;
}

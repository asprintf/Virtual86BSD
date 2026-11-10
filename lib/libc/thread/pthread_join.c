/* pthread_join, pthread_detach, pthread_self, pthread_equal */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"

/* the kernel writes DT_EXITED (0) and wakes us when the thread is gone;
 * that wake is a shared one, so the wait is too */
static int join(struct __pthread *t, void **res, int cp)
{
	int state;
	if (t == __pthread_self())
		return EDEADLK;
	while ((state = t->detach_state) != DT_EXITED) {
		if (state == DT_DETACHED)
			return EINVAL;
		if (cp)
			__timedwait_cp(&t->detach_state, state, 0, 0, 0);
		else
			__wait(&t->detach_state, state, 0);
	}
	a_barrier();
	if (res)
		*res = t->result;
	if (t->map_base)
		__syscall2(SYS_munmap, t->map_base, t->map_size);
	return 0;
}

int __pthread_join(__pthread_t t, void **res)
{
	return join(t, res, 1);
}

int __pthread_detach(__pthread_t t)
{
	int s = a_cas(&t->detach_state, DT_JOINABLE, DT_DETACHED);
	if (s == DT_JOINABLE)
		return 0;
	if (s == DT_DETACHED)
		return EINVAL;
	return join(t, 0, 0);      /* exiting or gone: reclaim it now */
}

__pthread_t __pthread_self_(void) { return __pthread_self(); }
int __pthread_equal(__pthread_t a, __pthread_t b) { return a == b; }

extern __typeof(__pthread_join) pthread_join __attribute__((weak, alias("__pthread_join")));
extern __typeof(__pthread_detach) pthread_detach __attribute__((weak, alias("__pthread_detach")));
extern __typeof(__pthread_self_) pthread_self __attribute__((weak, alias("__pthread_self_")));
extern __typeof(__pthread_equal) pthread_equal __attribute__((weak, alias("__pthread_equal")));

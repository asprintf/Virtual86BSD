/* Cancellation points (see pthread_impl.h).  Only reserved names here:
 * the C library's own functions (read() under fread(), say) come here. */
#include <errno.h>
#include "pthread_impl.h"
#include "syscall.h"

long __syscall_cp_asm(volatile int *, long, long, long, long, long, long, long);

/* act on a cancellation request: exit, or (masked) report ECANCELED */
long __cancel(void)
{
	struct __pthread *self = __pthread_self();
	if (self->canceldisable == 0 || self->cancelasync)
		__pthread_exit((void *)-1);   /* PTHREAD_CANCELED */
	self->canceldisable = 1;
	return -ECANCELED;
}

long __syscall_cp_c(long nr, long a, long b, long c, long d, long e, long f)
{
	struct __pthread *self = __pthread_self();
	int st = self->canceldisable;
	long r;
	if (st && (st == 1 || nr == SYS_close))
		return __syscall6x(nr, a, b, c, d, e, f);
	r = __syscall_cp_asm(&self->cancel, nr, a, b, c, d, e, f);
	if (r == -EINTR && nr != SYS_close && self->cancel && self->canceldisable != 1)
		r = __cancel();
	return r;
}

void __testcancel(void)
{
	struct __pthread *self = __pthread_self();
	if (self->cancel && !self->canceldisable)
		__cancel();
}

/* also accepts __CANCEL_MASKED */
int __setcancelstate(int new, int *old)
{
	struct __pthread *self = __pthread_self();
	if ((unsigned)new > 2)
		return EINVAL;
	if (old)
		*old = self->canceldisable;
	self->canceldisable = (unsigned char)new;
	return 0;
}

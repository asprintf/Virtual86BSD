/* pthread_cancel and the cancelability state (see pthread_impl.h). */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"
#include "syscall.h"
#include "atomic.h"
#include "lock.h"

extern const char __cp_begin[], __cp_end[], __cp_cancel[];

/* ucontext_t as longs: the program counter (gregs[REG_RIP]) and the
 * signal mask the kernel restores */
#define UC_RIP 21
#define UC_SIGMASK 37

static void cancel_handler(int sig, siginfo_t *si, void *ctx)
{
	struct __pthread *self = __pthread_self();
	unsigned long *uc = ctx, pc = uc[UC_RIP];
	(void)sig; (void)si;
	a_barrier();
	if (!self->cancel || self->canceldisable == 1)
		return;
	uc[UC_SIGMASK] |= 1UL << (SIGCANCEL - 1);
	if (self->cancelasync) {
		__restore_sigs(&uc[UC_SIGMASK]);
		__cancel();
	}
	if (pc >= (unsigned long)__cp_begin && pc < (unsigned long)__cp_end) {
		uc[UC_RIP] = (unsigned long)__cp_cancel;
		return;
	}
	/* not at a cancellation point: keep it pending (blocked) */
	__syscall2(SYS_tkill, self->tid, SIGCANCEL);
}

static volatile int installed, install_lock;

int pthread_cancel(__pthread_t t)
{
	struct __pthread *self = __pthread_self();
	if (!installed) {
		LOCK(&install_lock);
		if (!installed) {
			struct sigaction sa;
			memset(&sa, 0, sizeof sa);
			sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
			sa.sa_sigaction = cancel_handler;
			memset(&sa.sa_mask, -1, sizeof sa.sa_mask);
			__libc_sigaction(SIGCANCEL, &sa, 0);
			installed = 1;
		}
		UNLOCK(&install_lock);
	}
	a_store(&t->cancel, 1);
	if (t == self) {
		if (!self->canceldisable && self->cancelasync)
			__cancel();
		return 0;
	}
	return __pthread_kill(t, SIGCANCEL);
}

void pthread_testcancel(void)
{
	__testcancel();
}

int pthread_setcancelstate(int new, int *old)
{
	if ((unsigned)new > 1)
		return EINVAL;
	return __setcancelstate(new, old);
}

int pthread_setcanceltype(int new, int *old)
{
	struct __pthread *self = __pthread_self();
	if ((unsigned)new > 1)
		return EINVAL;
	if (old)
		*old = self->cancelasync;
	self->cancelasync = (unsigned char)new;
	if (new)
		__testcancel();
	return 0;
}

/* Recursive stream locks: internal operations nest inside flockfile(). */
#include "stdio_impl.h"
#include "pthread_impl.h"
#include "lock.h"
#include "atomic.h"

void __lockfile(FILE *f)
{
	int tid = __pthread_self()->tid;
	if (f->owner == tid) { f->lockcount++; return; }
	__lock(&f->lock);
	f->owner = tid;
	f->lockcount = 1;
}

void __unlockfile(FILE *f)
{
	if (--f->lockcount) return;
	f->owner = 0;
	__unlock(&f->lock);
}

int __trylockfile(FILE *f)
{
	int tid = __pthread_self()->tid;
	if (f->owner == tid) { f->lockcount++; return 0; }
	if (a_cas(&f->lock, 0, 1)) return -1;
	f->owner = tid;
	f->lockcount = 1;
	return 0;
}

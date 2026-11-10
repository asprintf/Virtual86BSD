/* The list of open (non-standard) streams. */
#include "stdio_impl.h"
#include "lock.h"

static FILE *head;
static volatile int lock;

FILE **__ofl_lock(void) { LOCK(&lock); return &head; }
void __ofl_unlock(void) { UNLOCK(&lock); }

FILE *__ofl_add(FILE *f)
{
	FILE **h = __ofl_lock();
	f->next = *h;
	if (*h) (*h)->prev = f;
	*h = f;
	__ofl_unlock();
	return f;
}

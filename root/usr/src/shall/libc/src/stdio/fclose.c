#include <stdlib.h>
#include "stdio_impl.h"

int fclose(FILE *f)
{
	int r, perm;
	FLOCK(f);
	r = __fflush_unlocked(f);
	r |= f->close(f);
	perm = f->flags & F_PERM;
	if (!perm) {
		FILE **head = __ofl_lock();
		if (f->prev) f->prev->next = f->next;
		if (f->next) f->next->prev = f->prev;
		if (*head == f) *head = f->next;
		__ofl_unlock();
	}
	FUNLOCK(f);
	if (!perm) {
		free(f->getln_buf);
		free(f);
	}
	return r;
}

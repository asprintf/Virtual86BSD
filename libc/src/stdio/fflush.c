#include <errno.h>
#include "stdio_impl.h"

/* Flush pending output; for input, give the unread bytes back to the
 * file position (POSIX), when the file is seekable. */
int __fflush_unlocked(FILE *f)
{
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return EOF;
	}
	if (f->rpos != f->rend) {
		int e = errno;
		if (f->seek(f, f->rpos - f->rend, SEEK_CUR) < 0) {
			if (errno != ESPIPE) return EOF;
			errno = e;   /* a pipe: the buffered input is dropped */
		}
	}
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;
	return 0;
}

int fflush(FILE *f)
{
	int r = 0;
	if (!f) {
		FILE *p;
		r = fflush(stdout) | fflush(stderr);
		for (p = *__ofl_lock(); p; p = p->next) {
			FLOCK(p);
			if (p->wpos != p->wbase) r |= __fflush_unlocked(p);
			FUNLOCK(p);
		}
		__ofl_unlock();
		return r;
	}
	FLOCK(f);
	r = __fflush_unlocked(f);
	FUNLOCK(f);
	return r;
}

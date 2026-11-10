/* The standard streams.  stdout is line buffered if it is a terminal
 * (decided on its first write), stderr unbuffered. */
#include "stdio_impl.h"
#include "libc.h"

static unsigned char inbuf[BUFSIZ + UNGET], outbuf[BUFSIZ + UNGET], errbuf[UNGET];

static FILE f_in = {
	.flags = F_PERM | F_NOWR, .buf = inbuf + UNGET, .buf_size = BUFSIZ,
	.fd = 0, .read = __stdio_read, .seek = __stdio_seek, .close = __stdio_close, .lbf = -1,
};
static FILE f_out = {
	.flags = F_PERM | F_NORD, .buf = outbuf + UNGET, .buf_size = BUFSIZ,
	.fd = 1, .write = __stdout_write, .seek = __stdio_seek, .close = __stdio_close, .lbf = '\n',
};
static FILE f_err = {
	.flags = F_PERM | F_NORD, .buf = errbuf + UNGET, .buf_size = 0,
	.fd = 2, .write = __stdio_write, .seek = __stdio_seek, .close = __stdio_close, .lbf = -1,
};

FILE *const stdin = &f_in;
FILE *const stdout = &f_out;
FILE *const stderr = &f_err;

/* exit(): flush the streams.  exit() calls this through a weak reference
 * (arch/x86_64/weak.s), so programs without stdio do not link it in. */
static void flush1(FILE *f)
{
	if (!f) return;
	FLOCK(f);
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	if (f->rpos != f->rend) f->seek(f, f->rpos - f->rend, SEEK_CUR);
	FUNLOCK(f);
}

void __stdio_exit(void)
{
	FILE *f;
	for (f = *__ofl_lock(); f; f = f->next) flush1(f);
	flush1(&f_in);
	flush1(&f_out);
	flush1(&f_err);
}

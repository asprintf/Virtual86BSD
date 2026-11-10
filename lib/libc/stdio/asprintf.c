/* POSIX printf variants. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

int vasprintf(char **__RESTRICT s, const char *__RESTRICT fmt, va_list ap)
{
	va_list ap2;
	int l;
	va_copy(ap2, ap);
	l = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (l < 0 || !(*s = malloc((size_t)l + 1))) { *s = 0; return -1; }  /* *s indeterminate per POSIX; null is safest */
	return vsnprintf(*s, (size_t)l + 1, fmt, ap);
}

int asprintf(char **__RESTRICT s, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vasprintf(s, fmt, ap); va_end(ap);
	return r;
}

static size_t fd_write(FILE *f, const unsigned char *s, size_t l)
{
	return __stdio_write(f, s, l);
}

int vdprintf(int fd, const char *__RESTRICT fmt, va_list ap)
{
	unsigned char buf[BUFSIZ + UNGET];
	FILE f;
	int r;
	memset(&f, 0, sizeof f);
	f.fd = fd;
	f.lbf = EOF;
	f.write = fd_write;
	f.buf = buf + UNGET;
	f.buf_size = BUFSIZ;
	f.lockcount = 1;
	r = __vfprintf(&f, fmt, ap);
	if (f.wpos != f.wbase) f.write(&f, 0, 0);
	return (f.flags & F_ERR) ? -1 : r;
}

int dprintf(int fd, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vdprintf(fd, fmt, ap); va_end(ap);
	return r;
}

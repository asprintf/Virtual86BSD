/* ioctl() (<sys/ioctl.h>, not POSIX): the argument is a pointer or an
 * integer, passed through. */
#include <stdarg.h>
#include <sys/ioctl.h>
#include "syscall.h"

int ioctl(int fd, unsigned long req, ...)
{
	va_list ap;
	void *arg;
	va_start(ap, req);
	arg = va_arg(ap, void *);
	va_end(ap);
	return (int)__sys3(SYS_ioctl, fd, req, arg);
}

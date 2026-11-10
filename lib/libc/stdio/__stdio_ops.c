/* The operations of a stream on a file descriptor. */
#include <errno.h>
#include <sys/uio.h>
#include "stdio_impl.h"
#include "syscall.h"

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
	struct iovec iov[2];
	long cnt;
	iov[0].iov_base = buf;
	iov[0].iov_len = len - !!f->buf_size;
	iov[1].iov_base = f->buf;
	iov[1].iov_len = f->buf_size;
	cnt = iov[0].iov_len ? __syscall_cp3(SYS_readv, f->fd, iov, 2)
	                     : __syscall_cp3(SYS_read, f->fd, iov[1].iov_base, iov[1].iov_len);
	if (cnt <= 0) {
		f->flags |= cnt ? F_ERR : F_EOF;
		if (cnt < 0) errno = (int)-cnt;
		return 0;
	}
	if ((size_t)cnt <= iov[0].iov_len) return (size_t)cnt;
	cnt -= (long)iov[0].iov_len;
	f->rpos = f->buf;
	f->rend = f->buf + cnt;
	if (f->buf_size) buf[len - 1] = *f->rpos++;
	return len;
}

size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct iovec iovs[2], *iov = iovs;
	size_t rem;
	int iovcnt = 2;
	long cnt;
	iovs[0].iov_base = f->wbase;
	iovs[0].iov_len = (size_t)(f->wpos - f->wbase);
	iovs[1].iov_base = (void *)buf;
	iovs[1].iov_len = len;
	rem = iov[0].iov_len + len;
	for (;;) {
		cnt = __syscall_cp3(SYS_writev, f->fd, iov, iovcnt);
		if ((size_t)cnt == rem) {
			f->wend = f->buf + f->buf_size;
			f->wpos = f->wbase = f->buf;
			return len;
		}
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			errno = (int)-cnt;
			return iovcnt == 2 ? 0 : len - iov[0].iov_len;
		}
		rem -= (size_t)cnt;
		if ((size_t)cnt > iov[0].iov_len) {
			cnt -= (long)iov[0].iov_len;
			iov++;
			iovcnt--;
		}
		iov[0].iov_base = (char *)iov[0].iov_base + cnt;
		iov[0].iov_len -= (size_t)cnt;
	}
}

/* stdout: line buffered only if it is a terminal (decided on first write) */
size_t __stdout_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct { unsigned short r, c, x, y; } ws;
	f->write = __stdio_write;
	if (!(f->flags & F_SVB) && __syscall3(SYS_ioctl, f->fd, 0x5413 /* TIOCGWINSZ */, &ws))
		f->lbf = -1;
	return __stdio_write(f, buf, len);
}

off_t __stdio_seek(FILE *f, off_t off, int whence)
{
	return __sys3(SYS_lseek, f->fd, off, whence);
}

int __stdio_close(FILE *f)
{
	long r = __syscall1(SYS_close, f->fd);
	if (r == -EINTR) r = 0;
	return (int)__syscall_ret((unsigned long)r);
}

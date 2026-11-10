#include <bits/features.h>
#if __POSIX_VISIBLE   /* (struct sigevent is POSIX: nothing in strict ISO C) */
#ifndef _AIO_H
#define _AIO_H

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#define __NEED_off_t
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_pthread_attr_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

struct aiocb {
	int aio_fildes;
	int aio_lio_opcode;
	int aio_reqprio;
	int __err;
	volatile void *aio_buf;
	size_t aio_nbytes;
	struct sigevent aio_sigevent;
	off_t aio_offset;
	ssize_t __ret;
	void *__req;
	long __reserved[5];
};

#define AIO_CANCELED    0
#define AIO_NOTCANCELED 1
#define AIO_ALLDONE     2

#define LIO_READ  0
#define LIO_WRITE 1
#define LIO_NOP   2

#define LIO_WAIT   0
#define LIO_NOWAIT 1

__BEGIN_DECLS
int aio_cancel(int, struct aiocb *);
int aio_error(const struct aiocb *);
int aio_fsync(int, struct aiocb *);
int aio_read(struct aiocb *);
ssize_t aio_return(struct aiocb *);
int aio_suspend(const struct aiocb *const [], int, const struct timespec *);
int aio_write(struct aiocb *);
int lio_listio(int, struct aiocb *__RESTRICT const [__RESTRICT], int, struct sigevent *__RESTRICT);
__END_DECLS

#endif /* __POSIX_VISIBLE */
#endif

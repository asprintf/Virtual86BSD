/* <sys/socket.h>.  SOCK_CLOFORK and MSG_CMSG_CLOFORK are emulated
 * (clofork.h).  struct msghdr and struct cmsghdr have the POSIX types
 * (int msg_iovlen, socklen_t msg_controllen and cmsg_len), where the
 * kernel reads 64-bit fields: the padding is cleared on the way in. */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "syscall.h"
#include "clofork.h"

#define TYPEFLAGS (SOCK_NONBLOCK | SOCK_CLOEXEC)

static int clofork(int fd, int on)
{
	if (fd >= 0 && on && __clofork_set(fd, 1) < 0) {
		__syscall1(SYS_close, fd);
		errno = ENOMEM;
		return -1;
	}
	return fd;
}

int socket(int domain, int type, int proto)
{
	int fd = (int)__sys3(SYS_socket, domain, type & ~SOCK_CLOFORK, proto);
	return clofork(fd, type & SOCK_CLOFORK);
}

int socketpair(int domain, int type, int proto, int fd[2])
{
	if (__sys4(SYS_socketpair, domain, type & ~SOCK_CLOFORK, proto, fd) < 0)
		return -1;
	if (type & SOCK_CLOFORK) {
		if (__clofork_set(fd[0], 1) < 0 || __clofork_set(fd[1], 1) < 0) {
			__syscall1(SYS_close, fd[0]);
			__syscall1(SYS_close, fd[1]);
			errno = ENOMEM;
			return -1;
		}
	}
	return 0;
}

int bind(int fd, const struct sockaddr *a, socklen_t n) { return (int)__sys3(SYS_bind, fd, a, n); }
int connect(int fd, const struct sockaddr *a, socklen_t n) { return (int)__sys_cp3(SYS_connect, fd, a, n); }
int listen(int fd, int backlog) { return (int)__sys2(SYS_listen, fd, backlog); }

int accept(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n)
{
	return (int)__sys_cp4(SYS_accept4, fd, a, n, 0);
}

int accept4(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n, int flags)
{
	if (flags & ~(TYPEFLAGS | SOCK_CLOFORK)) {
		errno = EINVAL;
		return -1;
	}
	return clofork((int)__sys_cp4(SYS_accept4, fd, a, n, flags & TYPEFLAGS), flags & SOCK_CLOFORK);
}

int getsockname(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n) { return (int)__sys3(SYS_getsockname, fd, a, n); }
int getpeername(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n) { return (int)__sys3(SYS_getpeername, fd, a, n); }

int getsockopt(int fd, int level, int opt, void *__RESTRICT v, socklen_t *__RESTRICT n)
{
	return (int)__sys5(SYS_getsockopt, fd, level, opt, v, n);
}

int setsockopt(int fd, int level, int opt, const void *v, socklen_t n)
{
	return (int)__sys5(SYS_setsockopt, fd, level, opt, v, n);
}

int shutdown(int fd, int how) { return (int)__sys2(SYS_shutdown, fd, how); }

int sockatmark(int fd)
{
	int r;
	if (__sys3(SYS_ioctl, fd, SIOCATMARK, &r) < 0)
		return -1;
	return r;
}

ssize_t sendto(int fd, const void *b, size_t n, int flags, const struct sockaddr *a, socklen_t an)
{
	return __sys_cp6(SYS_sendto, fd, b, n, flags, a, an);
}

ssize_t send(int fd, const void *b, size_t n, int flags) { return sendto(fd, b, n, flags, 0, 0); }

ssize_t recvfrom(int fd, void *__RESTRICT b, size_t n, int flags, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT an)
{
	return __sys_cp6(SYS_recvfrom, fd, b, n, flags, a, an);
}

ssize_t recv(int fd, void *b, size_t n, int flags) { return recvfrom(fd, b, n, flags, 0, 0); }

/* the kernel's struct msghdr */
struct kmsghdr {
	void *name;
	unsigned long namelen_unused;   /* int namelen + pad: as ours */
	struct iovec *iov;
	unsigned long iovlen;
	void *control;
	unsigned long controllen;
	unsigned long flags;
};

static void kmsg(struct kmsghdr *k, const struct msghdr *m)
{
	k->name = m->msg_name;
	k->namelen_unused = m->msg_namelen;
	k->iov = m->msg_iov;
	k->iovlen = (unsigned long)(unsigned)m->msg_iovlen;
	k->control = m->msg_control;
	k->controllen = m->msg_controllen;
	k->flags = (unsigned)m->msg_flags;
}

ssize_t sendmsg(int fd, const struct msghdr *m, int flags)
{
	struct kmsghdr k;
	unsigned char buf[1024];
	struct cmsghdr *c;
	struct msghdr mm;
	kmsg(&k, m);
	if (m->msg_controllen) {
		/* each cmsg_len is 64-bit for the kernel: clear the padding */
		if (m->msg_controllen > sizeof buf) {
			errno = ENOMEM;
			return -1;
		}
		memcpy(buf, m->msg_control, m->msg_controllen);
		mm = *m;
		mm.msg_control = buf;
		for (c = CMSG_FIRSTHDR(&mm); c; c = CMSG_NXTHDR(&mm, c))
			c->__pad1 = 0;
		k.control = buf;
	}
	return __sys_cp3(SYS_sendmsg, fd, &k, flags);
}

ssize_t recvmsg(int fd, struct msghdr *m, int flags)
{
	struct kmsghdr k;
	struct cmsghdr *c;
	ssize_t r;
	kmsg(&k, m);
	r = __sys_cp3(SYS_recvmsg, fd, &k, flags & ~MSG_CMSG_CLOFORK);
	if (r < 0)
		return r;
	m->msg_namelen = (socklen_t)k.namelen_unused;
	m->msg_controllen = (socklen_t)k.controllen;
	m->msg_flags = (int)k.flags;
	for (c = CMSG_FIRSTHDR(m); c; c = CMSG_NXTHDR(m, c)) {
		c->__pad1 = 0;
		if ((flags & MSG_CMSG_CLOFORK) && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
			int *fds = (int *)CMSG_DATA(c);
			size_t i, n = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			for (i = 0; i < n; i++)
				__clofork_set(fds[i], 1);
		}
	}
	return r;
}

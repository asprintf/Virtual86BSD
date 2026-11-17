/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* <sys/socket.h>.  SOCK_CLOFORK and MSG_CMSG_CLOFORK go to a kernel that
 * has close-on-fork, else they are emulated (clofork.h).  struct msghdr
 * and struct cmsghdr have the POSIX types (int msg_iovlen, socklen_t msg_controllen and cmsg_len), where the
 * kernel reads 64-bit fields: the padding is cleared on the way in. */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "syscall.h"

#define TYPEFLAGS (SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_CLOFORK)

int socket(int domain, int type, int proto)
{
	return (int)__sys3(SYS_socket, domain, type, proto);
}

int socketpair(int domain, int type, int proto, int fd[2])
{
	return (int)__sys4(SYS_socketpair, domain, type, proto, fd);
}

int bind(int fd, const struct sockaddr *a, socklen_t n) { return (int)__sys3(SYS_bind, fd, a, n); }
static int sotype(int fd)
{
	int t;
	socklen_t l = sizeof t;
	return __sys5(SYS_getsockopt, fd, SOL_SOCKET, SO_TYPE, &t, &l) < 0 ? -1 : t;
}

/* the kernel has already dissolved the association when it refuses AF_UNSPEC */
int connect(int fd, const struct sockaddr *a, socklen_t n)
{
	int r = (int)__sys_cp3(SYS_connect, fd, a, n);
	if (r < 0 && errno == EAFNOSUPPORT && a && a->sa_family == AF_UNSPEC) {
		int e = errno, t = sotype(fd);
		errno = e;
		if (t == SOCK_DGRAM)
			return 0;
	}
	return r;
}

/* the kernel says EINVAL for a socket that is not listening, whatever its type */
static int accepterr(int fd, int r)
{
	if (r < 0 && errno == EINVAL) {
		int t = sotype(fd);
		errno = EINVAL;
		if (t >= 0 && t != SOCK_STREAM && t != SOCK_SEQPACKET)
			errno = EOPNOTSUPP;
	}
	return r;
}
int listen(int fd, int backlog) { return (int)__sys2(SYS_listen, fd, backlog); }

int accept(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n)
{
	return accepterr(fd, (int)__sys_cp3(SYS_accept, fd, a, n));
}

int accept4(int fd, struct sockaddr *__RESTRICT a, socklen_t *__RESTRICT n, int flags)
{
	if (flags & ~TYPEFLAGS) {
		errno = EINVAL;
		return -1;
	}
	return accepterr(fd, (int)__sys_cp4(SYS_accept4, fd, a, n, flags));
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

/* (the kernel's struct msghdr is ours) */
ssize_t sendmsg(int fd, const struct msghdr *m, int flags)
{
	return __sys_cp3(SYS_sendmsg, fd, m, flags);
}

ssize_t recvmsg(int fd, struct msghdr *m, int flags)
{
	return __sys_cp3(SYS_recvmsg, fd, m, flags);
}

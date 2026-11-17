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

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_socklen_t
#define __NEED_sa_family_t
#define __NEED_struct_iovec
#include <bits/alltypes.h>

/* FreeBSD's values and layouts (sys/sys/socket.h) */
struct sockaddr {
	unsigned char sa_len;
	sa_family_t sa_family;
	char sa_data[14];
};

struct sockaddr_storage {
	unsigned char ss_len;
	sa_family_t ss_family;
	char __ss_pad1[6];
	long __ss_align;
	char __ss_pad2[112];
};

struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen;
	void *msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};

struct cmsghdr {
	socklen_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
};

struct linger {
	int l_onoff;
	int l_linger;
};

#define __CMSG_ALIGN(n) (((n) + sizeof(long) - 1) & ~(sizeof(long) - 1))
#define CMSG_DATA(c) ((unsigned char *)(c) + __CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(m) ((size_t)(m)->msg_controllen >= sizeof(struct cmsghdr) ? \
	(struct cmsghdr *)(m)->msg_control : (struct cmsghdr *)0)
#define CMSG_NXTHDR(m, c) \
	((c)->cmsg_len < sizeof(struct cmsghdr) || \
	 __CMSG_ALIGN((c)->cmsg_len) + sizeof(struct cmsghdr) >= \
	 (size_t)((unsigned char *)(m)->msg_control + (m)->msg_controllen - (unsigned char *)(c)) \
	 ? (struct cmsghdr *)0 : (struct cmsghdr *)((unsigned char *)(c) + __CMSG_ALIGN((c)->cmsg_len)))
#define CMSG_LEN(n) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + (n))
#define CMSG_SPACE(n) (__CMSG_ALIGN(n) + __CMSG_ALIGN(sizeof(struct cmsghdr)))

#define SCM_RIGHTS 1
#if __EXT_VISIBLE
#define SCM_CREDS 3
#endif

#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC   0x10000000
#define SOCK_NONBLOCK  0x20000000
#define SOCK_CLOFORK   0x40000000

#define SOL_SOCKET 0xffff

#define SO_DEBUG      0x0001
#define SO_ACCEPTCONN 0x0002
#define SO_REUSEADDR  0x0004
#define SO_KEEPALIVE  0x0008
#define SO_DONTROUTE  0x0010
#define SO_BROADCAST  0x0020
#define SO_LINGER     0x0080
#define SO_OOBINLINE  0x0100
#define SO_SNDBUF     0x1001
#define SO_RCVBUF     0x1002
#define SO_SNDLOWAT   0x1003
#define SO_RCVLOWAT   0x1004
#define SO_SNDTIMEO   0x1005
#define SO_RCVTIMEO   0x1006
#define SO_ERROR      0x1007
#define SO_TYPE       0x1008
#define SO_PROTOCOL   0x1016
#define SO_DOMAIN     0x1019
#if __EXT_VISIBLE
#define SO_REUSEPORT  0x0200
#endif

#define SOMAXCONN 128

#define MSG_OOB          0x1
#define MSG_PEEK         0x2
#define MSG_DONTROUTE    0x4
#define MSG_EOR          0x8
#define MSG_TRUNC        0x10
#define MSG_CTRUNC       0x20
#define MSG_WAITALL      0x40
#define MSG_NOSIGNAL     0x20000
#define MSG_CMSG_CLOEXEC 0x40000
#define MSG_CMSG_CLOFORK 0x400000
/* not POSIX: in the namespace XSI reserves (MSG_, PF_) */
#if __XSI_VISIBLE
#define MSG_DONTWAIT     0x80
#endif

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  28
#if __XSI_VISIBLE
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#endif
#if __EXT_VISIBLE
#define AF_LOCAL   AF_UNIX
#define PF_LOCAL   AF_UNIX
/* the routing table (sysctl, routing sockets), and link level addresses
   (struct sockaddr_dl in <net/if_dl.h>) */
#define AF_ROUTE 17
#define PF_ROUTE AF_ROUTE
#define AF_LINK 18
#define PF_LINK AF_LINK
#endif

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

__BEGIN_DECLS
int accept(int, struct sockaddr *__RESTRICT, socklen_t *__RESTRICT);
int accept4(int, struct sockaddr *__RESTRICT, socklen_t *__RESTRICT, int);
int bind(int, const struct sockaddr *, socklen_t);
int connect(int, const struct sockaddr *, socklen_t);
int getpeername(int, struct sockaddr *__RESTRICT, socklen_t *__RESTRICT);
int getsockname(int, struct sockaddr *__RESTRICT, socklen_t *__RESTRICT);
int getsockopt(int, int, int, void *__RESTRICT, socklen_t *__RESTRICT);
int listen(int, int);
ssize_t recv(int, void *, size_t, int);
ssize_t recvfrom(int, void *__RESTRICT, size_t, int, struct sockaddr *__RESTRICT, socklen_t *__RESTRICT);
ssize_t recvmsg(int, struct msghdr *, int);
ssize_t send(int, const void *, size_t, int);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
int setsockopt(int, int, int, const void *, socklen_t);
int shutdown(int, int);
int sockatmark(int);
int socket(int, int, int);
int socketpair(int, int, int, int[2]);
__END_DECLS

#endif

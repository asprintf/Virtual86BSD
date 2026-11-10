#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_socklen_t
#define __NEED_sa_family_t
#define __NEED_struct_iovec
#include <bits/alltypes.h>

/* Linux x86-64 values and layouts */
struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};

struct sockaddr_storage {
	sa_family_t ss_family;
	char __ss_pad[118];
	unsigned long __ss_align;
};

struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen;
	int __pad1;
	void *msg_control;
	socklen_t msg_controllen;
	int __pad2;
	int msg_flags;
};

struct cmsghdr {
	socklen_t cmsg_len;
	int __pad1;
	int cmsg_level;
	int cmsg_type;
};

struct linger {
	int l_onoff;
	int l_linger;
};

#define __CMSG_ALIGN(n) (((n) + sizeof(long) - 1) & ~(sizeof(long) - 1))
#define CMSG_DATA(c) ((unsigned char *)((struct cmsghdr *)(c) + 1))
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
#define SCM_CREDENTIALS 2
#endif

#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_SEQPACKET 5
#define SOCK_NONBLOCK  04000
#define SOCK_CLOEXEC   02000000
#define SOCK_CLOFORK   010000000000   /* native on V86BSD's kernel, else emulated */

#define SOL_SOCKET 1

#define SO_DEBUG      1
#define SO_REUSEADDR  2
#define SO_TYPE       3
#define SO_ERROR      4
#define SO_DONTROUTE  5
#define SO_BROADCAST  6
#define SO_SNDBUF     7
#define SO_RCVBUF     8
#define SO_KEEPALIVE  9
#define SO_OOBINLINE  10
#define SO_LINGER     13
#define SO_RCVLOWAT   18
#define SO_SNDLOWAT   19
#define SO_RCVTIMEO   20
#define SO_SNDTIMEO   21
#define SO_ACCEPTCONN 30
#define SO_PROTOCOL   38
#define SO_DOMAIN     39
#if __EXT_VISIBLE
#define SO_REUSEPORT  15
#define SO_PASSCRED   16
#define SO_PEERCRED   17
#endif

#define SOMAXCONN 4096

#define MSG_OOB          0x1
#define MSG_PEEK         0x2
#define MSG_DONTROUTE    0x4
#define MSG_CTRUNC       0x8
#define MSG_TRUNC        0x20
#define MSG_EOR          0x80
#define MSG_WAITALL      0x100
#define MSG_NOSIGNAL     0x4000
#define MSG_CMSG_CLOEXEC 0x40000000
#define MSG_CMSG_CLOFORK ((int)0x80000000)   /* native on V86BSD's kernel, else emulated */
/* not POSIX: in the namespace XSI reserves (MSG_, PF_) */
#if __XSI_VISIBLE
#define MSG_DONTWAIT     0x40
#endif

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10
#if __XSI_VISIBLE
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#endif
#if __EXT_VISIBLE
#define AF_LOCAL   AF_UNIX
#define PF_LOCAL   AF_UNIX
#define AF_NETLINK 16
#define PF_NETLINK AF_NETLINK
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

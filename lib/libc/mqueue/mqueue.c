/* <mqueue.h> on the kernel's message queues.  mq_notify with
 * SIGEV_THREAD: the kernel writes a notification record to a netlink
 * socket, which a thread of ours reads before calling the function. */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "syscall.h"
#include "pthread_impl.h"
#include "atomic.h"

mqd_t mq_open(const char *name, int flags, ...)
{
	mode_t mode = 0;
	struct mq_attr *attr = 0;
	if (*name == '/')
		name++;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		attr = va_arg(ap, struct mq_attr *);
		va_end(ap);
	}
	return (mqd_t)__sys4(SYS_mq_open, name, flags, mode, attr);
}

int mq_close(mqd_t q) { return close(q); }

int mq_unlink(const char *name)
{
	long r;
	if (*name == '/')
		name++;
	r = __syscall1(SYS_mq_unlink, name);
	if (r == -EPERM)
		r = -EACCES;          /* POSIX: EACCES */
	return (int)__syscall_ret((unsigned long)r);
}

int mq_getattr(mqd_t q, struct mq_attr *a) { return (int)__sys3(SYS_mq_getsetattr, q, 0, a); }

int mq_setattr(mqd_t q, const struct mq_attr *__RESTRICT a, struct mq_attr *__RESTRICT old)
{
	return (int)__sys3(SYS_mq_getsetattr, q, a, old);
}

int mq_timedsend(mqd_t q, const char *m, size_t n, unsigned prio, const struct timespec *at)
{
	return (int)__sys_cp5(SYS_mq_timedsend, q, m, n, prio, at);
}

int mq_send(mqd_t q, const char *m, size_t n, unsigned prio) { return mq_timedsend(q, m, n, prio, 0); }

ssize_t mq_timedreceive(mqd_t q, char *__RESTRICT m, size_t n, unsigned *__RESTRICT prio,
                        const struct timespec *__RESTRICT at)
{
	return __sys_cp5(SYS_mq_timedreceive, q, m, n, prio, at);
}

ssize_t mq_receive(mqd_t q, char *m, size_t n, unsigned *prio) { return mq_timedreceive(q, m, n, prio, 0); }

#define NOTIFY_COOKIE_LEN 32
#define NOTIFY_WOKENUP 1

struct ksigevent {
	union sigval value;
	int signo, notify, tid;
	int pad[11];
};

struct start {
	int sock;
	void (*fn)(union sigval);
	union sigval val;
	volatile int ready;
};

static void *notify_thread(void *p)
{
	struct start *s = p;
	int sock = s->sock;
	void (*fn)(union sigval) = s->fn;
	union sigval val = s->val;
	char buf[NOTIFY_COOKIE_LEN];
	ssize_t n;
	a_store(&s->ready, 1);
	__wake(&s->ready, 1, 1);
	n = recv(sock, buf, sizeof buf, MSG_NOSIGNAL | MSG_WAITALL);
	close(sock);
	if (n == sizeof buf && buf[sizeof buf - 1] == NOTIFY_WOKENUP)
		fn(val);
	return 0;
}

int mq_notify(mqd_t q, const struct sigevent *sev)
{
	struct ksigevent k;
	char attr[56], cookie[NOTIFY_COOKIE_LEN];
	long *a = (long *)attr;
	struct start s;
	struct __pthread *td;
	int r;

	if (!sev || sev->sigev_notify != SIGEV_THREAD)
		return (int)__sys2(SYS_mq_notify, q, sev);
	s.sock = socket(16 /* AF_NETLINK */, SOCK_RAW | SOCK_CLOEXEC, 0);
	if (s.sock < 0)
		return -1;
	s.fn = sev->sigev_notify_function;
	s.val = sev->sigev_value;
	s.ready = 0;
	if (sev->sigev_notify_attributes)
		memcpy(attr, sev->sigev_notify_attributes, sizeof attr);
	else {
		memset(attr, 0, sizeof attr);
		a[0] = 2L << 20;
		a[1] = 4096;
	}
	a[3] = 1;                       /* detached */
	if ((r = __pthread_create(&td, attr, notify_thread, &s))) {
		close(s.sock);
		errno = r;
		return -1;
	}
	while (!s.ready)
		__wait(&s.ready, 0, 1);
	memset(&k, 0, sizeof k);
	memset(cookie, 0, sizeof cookie);
	k.value.sival_ptr = cookie;
	k.signo = s.sock;
	k.notify = SIGEV_THREAD;
	if ((r = (int)__syscall2(SYS_mq_notify, q, &k)) < 0) {
		pthread_cancel(td);     /* in recv(), a cancellation point */
		close(s.sock);
		errno = -r;
		return -1;
	}
	return 0;
}

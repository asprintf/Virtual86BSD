#ifndef _MQUEUE_H
#define _MQUEUE_H
#include <bits/features.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_pthread_attr_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

typedef int mqd_t;

struct mq_attr {
	long mq_flags;
	long mq_maxmsg;
	long mq_msgsize;
	long mq_curmsgs;
	long __reserved[4];
};

__BEGIN_DECLS
int mq_close(mqd_t);
int mq_getattr(mqd_t, struct mq_attr *);
int mq_notify(mqd_t, const struct sigevent *);
mqd_t mq_open(const char *, int, ...);
ssize_t mq_receive(mqd_t, char *, size_t, unsigned *);
int mq_send(mqd_t, const char *, size_t, unsigned);
int mq_setattr(mqd_t, const struct mq_attr *__RESTRICT, struct mq_attr *__RESTRICT);
ssize_t mq_timedreceive(mqd_t, char *__RESTRICT, size_t, unsigned *__RESTRICT, const struct timespec *__RESTRICT);
int mq_timedsend(mqd_t, const char *, size_t, unsigned, const struct timespec *);
int mq_unlink(const char *);
__END_DECLS

#endif

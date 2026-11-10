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

/* mqueue.c - POSIX message queues (mq_open and friends), as Linux: a
   queue descriptor is a file descriptor; reading it gives the status line
   of Linux's mqueue file system. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define MQ_MAXMSG_DEF 10
#define MQ_MSGSIZE_DEF 8192
#define MQ_MAXMSG_MAX 65536
#define MQ_MSGSIZE_MAX (16 << 20)
#define MQ_PRIO_MAX 32768
#define NAME_MAX 255

#define SIGEV_SIGNAL 0
#define SIGEV_NONE 1
#define SIGEV_THREAD 2
#define NOTIFY_COOKIE_LEN 32

struct mqmsg {
  struct list link;
  unsigned prio;
  size_t len;
  char data[];
};

struct mq {
  struct list link; /* by name */
  char name[NAME_MAX + 1];
  bool unlinked;
  int ref;
  uid_t uid;
  gid_t gid;
  mode_t mode;
  long maxmsg, msgsize, curmsgs;
  uint64_t qsize;
  struct list msgs; /* by priority, then age */
  struct waitq wq;
  /* notification */
  struct proc *nproc;
  int nkind, nsigno;
  uint64_t nvalue;
  struct file *nsock;
  char cookie[NOTIFY_COOKIE_LEN];
  pid_t npid;
};

struct mqattr {
  int64_t flags, maxmsg, msgsize, curmsgs, pad[4];
};

static struct list queues = LIST_INIT(queues);
static struct super *mq_sb;
static const struct file_ops mq_fops;

int netlink_deliver(struct file *f, const void *data, size_t n);
bool is_netlink_file(struct file *f);

static void mq_put(struct mq *q) {
  if (--q->ref > 0 || !q->unlinked)
    return;
  list_for_each_safe(p, n, &q->msgs) {
    list_del(p);
    kfree(list_entry(p, struct mqmsg, link));
  }
  if (q->nsock)
    file_put(q->nsock);
  kfree(q);
}

static struct mq *find(const char *name) {
  list_for_each(p, &queues) {
    struct mq *q = list_entry(p, struct mq, link);
    if (!strcmp(q->name, name))
      return q;
  }
  return NULL;
}

static int perm(struct mq *q, int mask) {
  struct cred *c = &cur_proc()->cred;
  if (c->euid == 0)
    return 0;
  unsigned m = c->euid == q->uid ? (q->mode >> 6) & 7 : in_group(q->gid) ? (q->mode >> 3) & 7 : q->mode & 7;
  return (m & (unsigned)mask) == (unsigned)mask ? 0 : -EACCES;
}

static int get_name(long uname, char *out) {
  long n = strncpy_from_user(out, (const char *)uname, NAME_MAX + 1);
  if (n == -ENAMETOOLONG)
    return -ENAMETOOLONG;
  if (n < 0)
    return (int)n;
  if (n == 0 || strchr(out, '/'))
    return -EINVAL;
  return 0;
}

static struct mq *Q(struct file *f) { return f->priv; }

long sys_mq_open(long uname, long flags, long mode, long uattr) {
  char name[NAME_MAX + 1];
  int e = get_name(uname, name);
  if (e)
    return e;
  struct mq *q = find(name);
  if (q) {
    if ((flags & O_CREAT) && (flags & O_EXCL))
      return -EEXIST;
  } else {
    if (!(flags & O_CREAT))
      return -ENOENT;
    struct mqattr a = {0, MQ_MAXMSG_DEF, MQ_MSGSIZE_DEF, 0, {0}};
    if (uattr) {
      if (copy_from_user(&a, (void *)uattr, sizeof(a)))
        return -EFAULT;
      if (a.maxmsg <= 0 || a.msgsize <= 0)
        return -EINVAL;
      if ((a.maxmsg > MQ_MAXMSG_MAX || a.msgsize > MQ_MSGSIZE_MAX) && !capable())
        return -EINVAL;
    }
    q = kzalloc(sizeof(*q));
    if (!q)
      return -ENOMEM;
    strcpy(q->name, name);
    q->uid = cur_proc()->cred.euid;
    q->gid = cur_proc()->cred.egid;
    q->mode = (mode_t)mode & 0777 & ~cur_proc()->umask;
    q->maxmsg = a.maxmsg;
    q->msgsize = a.msgsize;
    list_init(&q->msgs);
    waitq_init(&q->wq);
    list_add_tail(&queues, &q->link);
  }
  int acc = (int)flags & O_ACCMODE;
  if (acc == 3)
    return -EINVAL;
  e = perm(q, acc == O_RDONLY ? MAY_READ : acc == O_WRONLY ? MAY_WRITE : MAY_READ | MAY_WRITE);
  if (e)
    return e;
  if (!mq_sb) {
    static struct fs_type t = {.name = "mqueue"};
    static const struct super_ops so = {.write_inode = NULL};
    mq_sb = super_alloc(&t, NULL);
    mq_sb->ops = &so;
    mq_sb->magic = 0x19800202;
    mq_sb->no_reopen = true;
  }
  struct inode *ino = new_inode(mq_sb, ++mq_sb->next_ino);
  if (!ino)
    return -ENOMEM;
  ino->mode = S_IFREG | q->mode;
  ino->nlink = 1;
  ino->uid = q->uid;
  ino->gid = q->gid;
  ino->fops = &mq_fops;
  struct file *f = file_alloc(ino, NULL, (unsigned)flags & (O_ACCMODE | O_NONBLOCK));
  iput(ino);
  if (!f)
    return -ENOMEM;
  f->priv = q;
  q->ref++;
  char p[NAME_MAX + 8];
  snprintf(p, sizeof(p), "/%s", q->name);
  f->path = kstrdup(p);
  int fd = fd_install(f, 0, fd_flags_of(flags));
  if (fd < 0)
    file_put(f);
  return fd;
}

long sys_mq_unlink(long uname) {
  char name[NAME_MAX + 1];
  int e = get_name(uname, name);
  if (e)
    return e;
  struct mq *q = find(name);
  if (!q)
    return -ENOENT;
  struct cred *c = &cur_proc()->cred;
  if (c->euid != 0 && c->euid != q->uid)
    return -EACCES;
  list_del(&q->link);
  q->unlinked = true;
  q->ref++;
  mq_put(q);
  return 0;
}

static struct mq *mq_of(long fd, struct file **fp) {
  struct file *f = fd_get((int)fd);
  if (!f || f->fops != &mq_fops)
    return NULL;
  *fp = f;
  return Q(f);
}

static uint64_t abs_deadline(long uts, int *err) {
  *err = 0;
  if (!uts)
    return 0;
  struct timespec64 ts;
  if (copy_from_user(&ts, (void *)uts, sizeof(ts))) {
    *err = -EFAULT;
    return 0;
  }
  if (ts.nsec < 0 || ts.nsec >= 1000000000) {
    *err = -EINVAL;
    return 0;
  }
  if (ts.sec < 0)
    return 1;
  uint64_t abs = ts_to_ns(ts.sec, ts.nsec);
  uint64_t now = realtime_ns();
  return abs > now ? sat_add(monotonic_ns(), abs - now) : 1;
}

static void notify(struct mq *q) {
  if (!q->nproc)
    return;
  /* only when the queue becomes non-empty and nobody waits to receive */
  if (q->nkind == SIGEV_SIGNAL && q->nsigno) {
    struct siginfo si;
    memset(&si, 0, sizeof(si));
    si.signo = q->nsigno;
    si.code = SI_MESGQ;
    si.u.kill.pid = cur_proc()->pid;
    si.u.kill.uid = cur_proc()->cred.uid;
    si.u.kill.value = q->nvalue;
    struct proc *p = find_proc(q->npid);
    if (p == q->nproc)
      send_signal_proc(p, &si);
  } else if (q->nkind == SIGEV_THREAD && q->nsock) {
    q->cookie[NOTIFY_COOKIE_LEN - 1] = 1; /* NOTIFY_WOKENUP */
    netlink_deliver(q->nsock, q->cookie, NOTIFY_COOKIE_LEN);
  }
  q->nproc = NULL;
  if (q->nsock) {
    file_put(q->nsock);
    q->nsock = NULL;
  }
}

long sys_mq_timedsend(long fd, long umsg, long len, long prio, long uts) {
  struct file *f;
  struct mq *q = mq_of(fd, &f);
  if (!q)
    return -EBADF;
  if ((f->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;
  if ((unsigned long)prio >= MQ_PRIO_MAX)
    return -EINVAL;
  if (len < 0 || len > q->msgsize)
    return -EMSGSIZE;
  int err;
  uint64_t dl = abs_deadline(uts, &err);
  struct mqmsg *m = kmalloc(sizeof(*m) + (size_t)len + 1);
  if (!m)
    return -ENOMEM;
  m->prio = (unsigned)prio;
  m->len = (size_t)len;
  if (copy_from_user(m->data, (void *)umsg, (size_t)len)) {
    kfree(m);
    return -EFAULT;
  }
  uint64_t fl = irq_save();
  while (q->curmsgs >= q->maxmsg) {
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      kfree(m);
      return -EAGAIN;
    }
    if (err) {
      irq_restore(fl);
      kfree(m);
      return err;
    }
    int r = sleep_on(&q->wq, dl, true);
    if (r) {
      irq_restore(fl);
      kfree(m);
      return r == -ETIMEDOUT ? -ETIMEDOUT : -EINTR;
    }
  }
  /* after the last message of the same or higher priority */
  struct list *pos = &q->msgs;
  list_for_each(p, &q->msgs) {
    if (list_entry(p, struct mqmsg, link)->prio < m->prio) {
      pos = p;
      break;
    }
  }
  list_add_tail(pos, &m->link);
  bool was_empty = q->curmsgs == 0;
  q->curmsgs++;
  q->qsize += (uint64_t)len;
  bool receivers = !list_empty(&q->wq.head);
  if (was_empty && !receivers)
    notify(q);
  wake_up(&q->wq);
  poll_wake();
  irq_restore(fl);
  return 0;
}

long sys_mq_timedreceive(long fd, long umsg, long len, long uprio, long uts) {
  struct file *f;
  struct mq *q = mq_of(fd, &f);
  if (!q)
    return -EBADF;
  if ((f->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  if (len < q->msgsize)
    return -EMSGSIZE;
  int err;
  uint64_t dl = abs_deadline(uts, &err);
  uint64_t fl = irq_save();
  while (q->curmsgs == 0) {
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      return -EAGAIN;
    }
    if (err) {
      irq_restore(fl);
      return err;
    }
    int r = sleep_on(&q->wq, dl, true);
    if (r) {
      irq_restore(fl);
      return r == -ETIMEDOUT ? -ETIMEDOUT : -EINTR;
    }
  }
  struct mqmsg *m = list_entry(q->msgs.next, struct mqmsg, link);
  list_del(&m->link);
  q->curmsgs--;
  q->qsize -= m->len;
  wake_up(&q->wq);
  poll_wake();
  irq_restore(fl);
  long r = (long)m->len;
  uint32_t pr = m->prio;
  if (copy_to_user((void *)umsg, m->data, m->len) || (uprio && copy_to_user((void *)uprio, &pr, 4)))
    r = -EFAULT;
  kfree(m);
  return r;
}

struct ksigevent {
  uint64_t value;
  int32_t signo, notify;
  int32_t tid;
  int32_t pad[11];
};

long sys_mq_notify(long fd, long usev) {
  struct file *f;
  struct mq *q = mq_of(fd, &f);
  if (!q)
    return -EBADF;
  struct proc *me = cur_proc();
  if (!usev) {
    if (q->nproc == me && q->npid == me->pid) {
      q->nproc = NULL;
      if (q->nsock) {
        file_put(q->nsock);
        q->nsock = NULL;
      }
    }
    return 0;
  }
  struct ksigevent ev;
  if (copy_from_user(&ev, (void *)usev, sizeof(ev)))
    return -EFAULT;
  if (ev.notify != SIGEV_SIGNAL && ev.notify != SIGEV_NONE && ev.notify != SIGEV_THREAD)
    return -EINVAL;
  if (ev.notify == SIGEV_SIGNAL && (ev.signo < 1 || ev.signo > NSIG))
    return -EINVAL;
  if (q->nproc && find_proc(q->npid) == q->nproc)
    return -EBUSY;
  if (ev.notify == SIGEV_THREAD) {
    struct file *s = fd_get(ev.signo);
    if (!is_netlink_file(s))
      return -EBADF;
    if (copy_from_user(q->cookie, (void *)ev.value, NOTIFY_COOKIE_LEN))
      return -EFAULT;
    file_get(s);
    q->nsock = s;
  }
  q->nproc = me;
  q->npid = me->pid;
  q->nkind = ev.notify;
  q->nsigno = ev.signo;
  q->nvalue = ev.value;
  return 0;
}

long sys_mq_getsetattr(long fd, long unew, long uold) {
  struct file *f;
  struct mq *q = mq_of(fd, &f);
  if (!q)
    return -EBADF;
  struct mqattr o = {(f->flags & O_NONBLOCK) ? O_NONBLOCK : 0, q->maxmsg, q->msgsize, q->curmsgs, {0}};
  if (unew) {
    struct mqattr n;
    if (copy_from_user(&n, (void *)unew, sizeof(n)))
      return -EFAULT;
    if (n.flags & ~(int64_t)O_NONBLOCK)
      return -EINVAL;
    if (n.flags & O_NONBLOCK)
      f->flags |= O_NONBLOCK;
    else
      f->flags &= ~(unsigned)O_NONBLOCK;
  }
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  return 0;
}

static ssize_t mq_read(struct file *f, char *buf, size_t n, off_t *pos) {
  struct mq *q = Q(f);
  char t[128];
  int l = snprintf(t, sizeof(t), "QSIZE:%-10lu NOTIFY:%-5d SIGNO:%-5d NOTIFY_PID:%-6d\n", q->qsize,
                   q->nproc ? q->nkind : 0, q->nproc ? q->nsigno : 0, q->nproc ? q->npid : 0);
  if (*pos >= l)
    return 0;
  size_t c = MIN(n, (size_t)(l - *pos));
  if (copy_to_user(buf, t + *pos, c))
    return -EFAULT;
  *pos += (off_t)c;
  return (ssize_t)c;
}

static unsigned mq_poll(struct file *f) {
  struct mq *q = Q(f);
  unsigned m = 0;
  if (q->curmsgs)
    m |= POLLIN | POLLRDNORM;
  if (q->curmsgs < q->maxmsg)
    m |= POLLOUT | POLLWRNORM;
  return m;
}

static void mq_release(struct file *f) {
  struct mq *q = Q(f);
  if (!q)
    return;
  /* closing removes this process's registration */
  if (q->nproc == cur_proc()) {
    bool other = false;
    struct proc *p = cur_proc();
    for (int i = 0; i < p->nfd; i++)
      if (p->fd[i] && p->fd[i] != f && p->fd[i]->priv == q && p->fd[i]->fops == &mq_fops)
        other = true;
    if (!other) {
      q->nproc = NULL;
      if (q->nsock) {
        file_put(q->nsock);
        q->nsock = NULL;
      }
    }
  }
  mq_put(q);
}

static const struct file_ops mq_fops = {.read = mq_read, .poll = mq_poll, .release = mq_release};

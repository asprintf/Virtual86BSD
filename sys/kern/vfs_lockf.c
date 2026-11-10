/* lock.c - POSIX record locks (fcntl F_GETLK/F_SETLK/F_SETLKW), owned by
   the process, and open-file-description locks (F_OFD_*), owned by the
   open file.  Closing any descriptor of a file drops the process's locks on
   it, as POSIX says. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define F_OFD_GETLK 36
#define F_OFD_SETLK 37
#define F_OFD_SETLKW 38
#define OFFSET_MAX 0x7fffffffffffffffL

struct lock {
  struct list link;
  void *owner; /* struct proc * or struct file * */
  bool ofd;
  pid_t pid;
  int type;
  int64_t start, end; /* inclusive */
};

struct kflock {
  int16_t type, whence;
  int32_t pad;
  int64_t start, len;
  int32_t pid, pad2;
};

/* who a blocked process waits for (deadlock detection) */
struct waiter {
  struct list link;
  void *owner, *waits_for;
};
static struct list waiters = LIST_INIT(waiters);

static bool conflicts(struct lock *a, void *owner, int type, int64_t s, int64_t e) {
  if (a->owner == owner)
    return false;
  if (a->end < s || a->start > e)
    return false;
  return a->type == F_WRLCK || type == F_WRLCK;
}

static struct lock *find_conflict(struct inode *ino, void *owner, int type, int64_t s, int64_t e) {
  list_for_each(p, &ino->locks) {
    struct lock *l = list_entry(p, struct lock, link);
    if (conflicts(l, owner, type, s, e))
      return l;
  }
  return NULL;
}

static bool would_deadlock(void *me, void *blocker) {
  for (int n = 0; n < 64 && blocker; n++) {
    if (blocker == me)
      return true;
    void *next = NULL;
    list_for_each(p, &waiters) {
      struct waiter *w = list_entry(p, struct waiter, link);
      if (w->owner == blocker) {
        next = w->waits_for;
        break;
      }
    }
    blocker = next;
  }
  return false;
}

/* remove owner's locks in [s, e], splitting at the edges */
static int unlock_range(struct inode *ino, void *owner, int64_t s, int64_t e) {
  list_for_each_safe(p, n, &ino->locks) {
    struct lock *l = list_entry(p, struct lock, link);
    if (l->owner != owner || l->end < s || l->start > e)
      continue;
    if (l->start < s && l->end > e) {
      struct lock *r = kmalloc(sizeof(*r));
      if (!r)
        return -ENOLCK;
      *r = *l;
      r->start = e + 1;
      list_add(&l->link, &r->link);
      l->end = s - 1;
    } else if (l->start < s)
      l->end = s - 1;
    else if (l->end > e)
      l->start = e + 1;
    else {
      list_del(&l->link);
      kfree(l);
    }
  }
  return 0;
}

long do_fcntl_lock(struct file *f, int cmd, void *ufl) {
  struct kflock fl;
  if (copy_from_user(&fl, ufl, sizeof(fl)))
    return -EFAULT;
  bool ofd = cmd == F_OFD_GETLK || cmd == F_OFD_SETLK || cmd == F_OFD_SETLKW;
  if (ofd && fl.pid != 0)
    return -EINVAL;
  void *owner = ofd ? (void *)f : (void *)cur_proc();
  struct inode *ino = f->ino;
  int64_t base;
  switch (fl.whence) {
  case 0: base = 0; break;
  case 1: base = f->pos; break;
  case 2: base = (int64_t)ino->size; break;
  default: return -EINVAL;
  }
  int64_t s = base + fl.start, e;
  if (fl.len > 0) {
    if (fl.start > 0 && base > OFFSET_MAX - fl.start)
      return -EOVERFLOW;
    e = s + fl.len - 1;
  } else if (fl.len < 0) {
    e = s - 1;
    s += fl.len;
  } else
    e = OFFSET_MAX;
  if (s < 0)
    return -EINVAL;
  if (fl.type != F_RDLCK && fl.type != F_WRLCK && fl.type != F_UNLCK)
    return -EINVAL;
  if (cmd == F_GETLK || cmd == F_OFD_GETLK) {
    /* F_UNLCK asked about: nothing would block it, so F_UNLCK (as Linux
       and the BSDs answer; os-test expects it) */
    struct lock *c = fl.type == F_UNLCK ? NULL : find_conflict(ino, owner, fl.type, s, e);
    if (!c)
      fl.type = F_UNLCK;
    else {
      fl.type = (int16_t)c->type;
      fl.whence = 0;
      fl.start = c->start;
      fl.len = c->end == OFFSET_MAX ? 0 : c->end - c->start + 1;
      fl.pid = c->ofd ? -1 : c->pid;
    }
    return copy_to_user(ufl, &fl, sizeof(fl)) ? -EFAULT : 0;
  }
  unsigned acc = f->flags & O_ACCMODE;
  if ((fl.type == F_RDLCK && acc == O_WRONLY) || (fl.type == F_WRLCK && acc == O_RDONLY))
    return -EBADF;
  if (fl.type == F_UNLCK)
    return unlock_range(ino, owner, s, e);
  bool wait = cmd == F_SETLKW || cmd == F_OFD_SETLKW;
  uint64_t flags = irq_save();
  struct lock *c;
  while ((c = find_conflict(ino, owner, fl.type, s, e))) {
    if (!wait) {
      irq_restore(flags);
      return -EAGAIN;
    }
    if (would_deadlock(owner, c->owner)) {
      irq_restore(flags);
      return -EDEADLK;
    }
    struct waiter w = {{0, 0}, owner, c->owner};
    list_add_tail(&waiters, &w.link);
    int r = sleep_on(&ino->lockwait, 0, true);
    list_del(&w.link);
    if (r == -EINTR) {
      irq_restore(flags);
      return -EINTR;
    }
  }
  irq_restore(flags);
  int r = unlock_range(ino, owner, s, e);
  if (r)
    return r;
  struct lock *l = kmalloc(sizeof(*l));
  if (!l)
    return -ENOLCK;
  l->owner = owner;
  l->ofd = ofd;
  l->pid = cur_proc()->pid;
  l->type = fl.type;
  l->start = s;
  l->end = e;
  list_add_tail(&ino->locks, &l->link);
  wake_up(&ino->lockwait);
  return 0;
}

void locks_release(struct proc *p, struct inode *ino) {
  bool any = false;
  list_for_each_safe(q, n, &ino->locks) {
    struct lock *l = list_entry(q, struct lock, link);
    if (l->owner == p) {
      list_del(&l->link);
      kfree(l);
      any = true;
    }
  }
  if (any)
    wake_up(&ino->lockwait);
}

/* an open file description's locks go with its last reference */
void ofd_locks_release(struct file *f) {
  if (!f->ino)
    return;
  bool any = false;
  list_for_each_safe(q, n, &f->ino->locks) {
    struct lock *l = list_entry(q, struct lock, link);
    if (l->owner == f) {
      list_del(&l->link);
      kfree(l);
      any = true;
    }
  }
  if (any)
    wake_up(&f->ino->lockwait);
}

long sys_flock(long fd, long op) {
  (void)op;
  return fd_get((int)fd) ? 0 : -EBADF;
}

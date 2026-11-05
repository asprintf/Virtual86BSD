/* pipe.c - pipes and FIFOs: a 64 KiB ring buffer per pipe. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "syscall.h"

#define PIPE_SIZE 65536
#define PIPE_BUF 4096

struct pipe {
  char *buf;
  size_t head, len;
  int readers, writers;
  struct waitq rq, wq, oq; /* readers, writers, FIFO opens */
  unsigned opens_r, opens_w; /* counters for FIFO open handshakes */
};

static struct super *pipe_sb;
static const struct super_ops pipe_sops;
static const struct file_ops pipe_fops;

static struct pipe *pipe_new(void) {
  struct pipe *p = kzalloc(sizeof(*p));
  if (!p)
    return NULL;
  p->buf = kmalloc(PIPE_SIZE);
  if (!p->buf) {
    kfree(p);
    return NULL;
  }
  waitq_init(&p->rq);
  waitq_init(&p->wq);
  waitq_init(&p->oq);
  return p;
}

void pipe_free_inode(struct inode *ino) {
  struct pipe *p = ino->pipe;
  if (p && p->readers == 0 && p->writers == 0) {
    kfree(p->buf);
    kfree(p);
    ino->pipe = NULL;
  }
}

static bool is_write_end(struct file *f) { return (f->flags & O_ACCMODE) != O_RDONLY; }
static bool is_read_end(struct file *f) { return (f->flags & O_ACCMODE) != O_WRONLY; }

static ssize_t pipe_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct pipe *p = f->ino->pipe;
  if (n == 0)
    return 0;
  uint64_t fl = irq_save();
  while (p->len == 0) {
    if (p->writers == 0) {
      irq_restore(fl);
      return 0;
    }
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      return -EAGAIN;
    }
    if (sleep_on(&p->rq, 0, true) == -EINTR) {
      irq_restore(fl);
      return -ERESTARTSYS;
    }
  }
  size_t done = 0;
  while (done < n && p->len) {
    size_t chunk = MIN(n - done, MIN(p->len, PIPE_SIZE - p->head));
    if (copy_to_user(buf + done, p->buf + p->head, chunk)) {
      if (!done)
        done = (size_t)-EFAULT;
      break;
    }
    p->head = (p->head + chunk) % PIPE_SIZE;
    p->len -= chunk;
    done += chunk;
  }
  wake_up(&p->wq);
  poll_wake();
  irq_restore(fl);
  if ((ssize_t)done > 0)
    inode_set_times(f->ino, true, false, false);
  return (ssize_t)done;
}

static ssize_t pipe_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct pipe *p = f->ino->pipe;
  size_t done = 0;
  uint64_t fl = irq_save();
  while (done < n) {
    if (p->readers == 0) {
      irq_restore(fl);
      signal_simple(cur_proc(), SIGPIPE);
      return done ? (ssize_t)done : -EPIPE;
    }
    size_t space = PIPE_SIZE - p->len;
    /* writes of up to PIPE_BUF bytes are atomic */
    size_t want = n - done;
    if ((n <= PIPE_BUF && space < want) || space == 0) {
      if (f->flags & O_NONBLOCK) {
        irq_restore(fl);
        return done ? (ssize_t)done : -EAGAIN;
      }
      if (sleep_on(&p->wq, 0, true) == -EINTR) {
        irq_restore(fl);
        return done ? (ssize_t)done : -ERESTARTSYS;
      }
      continue;
    }
    size_t chunk = MIN(want, space);
    size_t tail = (p->head + p->len) % PIPE_SIZE;
    size_t c1 = MIN(chunk, PIPE_SIZE - tail);
    if (copy_from_user(p->buf + tail, buf + done, c1) ||
        (chunk > c1 && copy_from_user(p->buf, buf + done + c1, chunk - c1))) {
      irq_restore(fl);
      return done ? (ssize_t)done : -EFAULT;
    }
    p->len += chunk;
    done += chunk;
    wake_up(&p->rq);
    poll_wake();
  }
  irq_restore(fl);
  inode_set_times(f->ino, false, true, true);
  return (ssize_t)done;
}

static unsigned pipe_poll(struct file *f) {
  struct pipe *p = f->ino->pipe;
  unsigned m = 0;
  if (is_read_end(f)) {
    if (p->len)
      m |= POLLIN | POLLRDNORM;
    if (p->writers == 0 && (f->priv || !S_ISFIFO(f->ino->mode) || true))
      m |= POLLHUP;
  }
  if (is_write_end(f)) {
    if (PIPE_SIZE - p->len >= PIPE_BUF)
      m |= POLLOUT | POLLWRNORM;
    if (p->readers == 0)
      m |= POLLERR;
  }
  return m;
}

static void pipe_release(struct file *f) {
  struct pipe *p = f->ino->pipe;
  if (!p)
    return;
  if (is_read_end(f))
    p->readers--;
  if (is_write_end(f))
    p->writers--;
  wake_up(&p->rq);
  wake_up(&p->wq);
  poll_wake();
}

#define FIONREAD 0x541b

static long pipe_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  if (cmd == FIONREAD) {
    int32_t n = (int32_t)f->ino->pipe->len;
    return copy_to_user((void *)arg, &n, 4) ? -EFAULT : 0;
  }
  return -ENOTTY;
}

static const struct file_ops pipe_fops = {
    .read = pipe_read, .write = pipe_write, .poll = pipe_poll, .release = pipe_release, .ioctl = pipe_ioctl};

/* FIFOs: open blocks until the other side opens (O_NONBLOCK rules) */
int fifo_open(struct inode *ino, struct file *f) {
  if (!ino->pipe) {
    ino->pipe = pipe_new();
    if (!ino->pipe)
      return -ENOMEM;
  }
  struct pipe *p = ino->pipe;
  f->fops = &pipe_fops;
  unsigned acc = f->flags & O_ACCMODE;
  /* an anonymous pipe reopened through /proc: no waiting for the other
     end (as Linux's fifo_open for pipefs) */
  bool anon = ino->sb == pipe_sb;
  uint64_t fl = irq_save();
  if (acc == O_RDONLY) {
    p->readers++;
    p->opens_r++;
    wake_up(&p->oq);
    if (!(f->flags & O_NONBLOCK) && !anon) {
      unsigned w = p->opens_w;
      while (p->writers == 0 && p->opens_w == w) {
        if (sleep_on(&p->oq, 0, true) == -EINTR) {
          p->readers--;
          irq_restore(fl);
          return -ERESTARTSYS;
        }
      }
    }
  } else if (acc == O_WRONLY) {
    if ((f->flags & O_NONBLOCK) && p->readers == 0 && !anon) {
      irq_restore(fl);
      return -ENXIO;
    }
    p->writers++;
    p->opens_w++;
    wake_up(&p->oq);
    unsigned r = p->opens_r;
    while (!anon && p->readers == 0 && p->opens_r == r) {
      if (sleep_on(&p->oq, 0, true) == -EINTR) {
        p->writers--;
        irq_restore(fl);
        return -ERESTARTSYS;
      }
    }
  } else {
    p->readers++;
    p->writers++;
    p->opens_r++;
    p->opens_w++;
    wake_up(&p->oq);
  }
  irq_restore(fl);
  return 0;
}

const struct file_ops fifo_fops = {.open = fifo_open};

static int pipe_noop(struct inode *i) {
  (void)i;
  return 0;
}
static const struct super_ops pipe_sops = {.write_inode = pipe_noop};

long sys_pipe2(long ufds, long flags) {
  if (flags & ~(O_CLOEXEC | O_CLOFORK | O_NONBLOCK | O_DIRECT))
    return -EINVAL;
  if (!pipe_sb) {
    static struct fs_type pt = {.name = "pipefs"};
    pipe_sb = super_alloc(&pt, NULL);
    pipe_sb->ops = &pipe_sops;
    pipe_sb->blocksize = PAGE_SIZE;
    pipe_sb->magic = 0x50495045;
  }
  struct inode *ino = new_inode(pipe_sb, ++pipe_sb->next_ino);
  if (!ino)
    return -ENFILE;
  ino->mode = S_IFIFO | 0600;
  ino->nlink = 1;
  ino->uid = cur_proc()->cred.fsuid;
  ino->gid = cur_proc()->cred.fsgid;
  ino->fops = &fifo_fops; /* reopened through /proc: as a FIFO (Linux) */
  inode_set_times(ino, true, true, true);
  ino->pipe = pipe_new();
  if (!ino->pipe) {
    iput(ino);
    return -ENFILE;
  }
  ino->pipe->readers = ino->pipe->writers = 1;
  struct file *r = file_alloc(ino, NULL, O_RDONLY | ((unsigned)flags & O_NONBLOCK));
  struct file *w = file_alloc(ino, NULL, O_WRONLY | ((unsigned)flags & O_NONBLOCK));
  r->fops = w->fops = &pipe_fops;
  iput(ino); /* the files hold it */
  r->path = kstrdup("pipe:");
  w->path = kstrdup("pipe:");
  int fr = fd_install(r, 0, fd_flags_of(flags));
  if (fr < 0) {
    file_put(r);
    file_put(w);
    return fr;
  }
  int fw = fd_install(w, 0, fd_flags_of(flags));
  if (fw < 0) {
    fd_close(fr);
    file_put(w);
    return fw;
  }
  int32_t fds[2] = {fr, fw};
  if (copy_to_user((void *)ufds, fds, sizeof(fds))) {
    fd_close(fr);
    fd_close(fw);
    return -EFAULT;
  }
  return 0;
}

long sys_pipe(long ufds) { return sys_pipe2(ufds, 0); }

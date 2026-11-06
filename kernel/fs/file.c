/* file.c - open files, descriptor tables, and the system calls that work on
   descriptors: read/write family, lseek, dup, fcntl, ioctl, poll, select. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "dev.h"
#include "tty.h"
#include "syscall.h"

#define MAX_FD (1024 * 1024)

struct waitq poll_wait = WAITQ_INIT(poll_wait);

void poll_wake(void) { wake_up(&poll_wait); }

struct file *file_alloc(struct inode *ino, struct mount *mnt, unsigned flags) {
  struct file *f = kzalloc(sizeof(*f));
  if (!f)
    return NULL;
  f->ref = 1;
  f->ino = ino ? igrab(ino) : NULL;
  f->mnt = mnt;
  f->flags = flags;
  f->fops = ino ? ino->fops : NULL;
  return f;
}

void file_get(struct file *f) { f->ref++; }

void file_put(struct file *f) {
  if (--f->ref > 0)
    return;
  if (f->fops && f->fops->release)
    f->fops->release(f);
  ofd_locks_release(f);
  if (f->ino)
    iput(f->ino);
  kfree(f->path);
  kfree(f);
}

struct file *fd_get(int fd) {
  struct proc *p = cur_proc();
  if (fd < 0 || fd >= p->nfd)
    return NULL;
  return p->fd[fd];
}

static int fd_grow(struct proc *p, int want) {
  if (want <= p->nfd)
    return 0;
  int n = p->nfd ? p->nfd : 64;
  while (n < want)
    n *= 2;
  struct file **nf = kzalloc(n * sizeof(*nf));
  uint8_t *nfl = kzalloc((size_t)n);
  if (!nf || !nfl) {
    kfree(nf);
    kfree(nfl);
    return -ENOMEM;
  }
  if (p->nfd) {
    memcpy(nf, p->fd, p->nfd * sizeof(*nf));
    memcpy(nfl, p->fdflags, (size_t)p->nfd);
  }
  kfree(p->fd);
  kfree(p->fdflags);
  p->fd = nf;
  p->fdflags = nfl;
  p->nfd = n;
  return 0;
}

/* install f (taking over the caller's reference) at the lowest free fd >= min */
int fd_install(struct file *f, int min, unsigned fdflags) {
  struct proc *p = cur_proc();
  int lim = (int)MIN(p->rlim[RLIMIT_NOFILE].cur, (uint64_t)MAX_FD);
  for (int fd = min; fd < lim; fd++) {
    if (fd >= p->nfd && fd_grow(p, fd + 1))
      return -ENOMEM;
    if (!p->fd[fd]) {
      p->fd[fd] = f;
      p->fdflags[fd] = (uint8_t)(fdflags & (FD_CLOEXEC | FD_CLOFORK));
      return fd;
    }
  }
  return -EMFILE;
}

int fd_close(int fd) {
  struct proc *p = cur_proc();
  struct file *f = fd_get(fd);
  if (!f)
    return -EBADF;
  p->fd[fd] = NULL;
  p->fdflags[fd] = 0;
  if (f->ino)
    locks_release(p, f->ino);
  file_put(f);
  return 0;
}

/* exec: the descriptors with FD_CLOEXEC are closed, and FD_CLOFORK is
   cleared on the others (Austin Group interpretation 1851, for TC1 of
   POSIX.1-2024: the flag belongs to the program that set it) */
void fd_close_on_exec(struct proc *p) {
  for (int i = 0; i < p->nfd; i++)
    if (p->fd[i] && !(p->fdflags[i] & FD_CLOEXEC))
      p->fdflags[i] &= (uint8_t)~FD_CLOFORK;
    else if (p->fd[i]) {
      struct file *f = p->fd[i];
      p->fd[i] = NULL;
      p->fdflags[i] = 0;
      if (f->ino)
        locks_release(p, f->ino);
      file_put(f);
    }
}

void fd_close_all(struct proc *p) {
  for (int i = 0; i < p->nfd; i++)
    if (p->fd[i]) {
      struct file *f = p->fd[i];
      p->fd[i] = NULL;
      if (f->ino)
        locks_release(p, f->ino);
      file_put(f);
    }
  kfree(p->fd);
  kfree(p->fdflags);
  p->fd = NULL;
  p->fdflags = NULL;
  p->nfd = 0;
}

/* a new process's descriptors: the parent's, but those with FD_CLOFORK */
int fds_dup(struct proc *dst, struct proc *src) {
  int last = -1;
  for (int i = 0; i < src->nfd; i++)
    if (src->fd[i] && !(src->fdflags[i] & FD_CLOFORK))
      last = i;
  dst->nfd = 0;
  dst->fd = NULL;
  dst->fdflags = NULL;
  if (last < 0)
    return 0;
  if (fd_grow(dst, last + 1))
    return -ENOMEM;
  for (int i = 0; i <= last; i++) {
    if (!src->fd[i] || (src->fdflags[i] & FD_CLOFORK))
      continue;
    dst->fd[i] = src->fd[i];
    dst->fdflags[i] = src->fdflags[i];
    file_get(dst->fd[i]);
  }
  return 0;
}

/* ---- kernel I/O helpers ---- */

ssize_t kernel_read(struct file *f, void *buf, size_t n, off_t off) {
  if (!f->fops || !f->fops->read)
    return -EINVAL;
  off_t pos = off;
  return f->fops->read(f, buf, n, &pos);
}

ssize_t kernel_write(struct file *f, const void *buf, size_t n, off_t off) {
  if (!f->fops || !f->fops->write)
    return -EINVAL;
  off_t pos = off;
  return f->fops->write(f, buf, n, &pos);
}

/* ---- opening ---- */

struct file *open_inode(struct inode *ino, struct mount *mnt, int flags, int *err) {
  if (ino->sb->no_reopen && !(flags & O_PATH)) {
    *err = -ENXIO;
    return NULL;
  }
  struct file *f = file_alloc(ino, mnt, (unsigned)flags & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC | O_CLOFORK));
  if (!f) {
    *err = -ENOMEM;
    return NULL;
  }
  if (flags & O_PATH) {
    f->fops = NULL;
    return f;
  }
  if (S_ISCHR(ino->mode) || S_ISBLK(ino->mode)) {
    if (mnt && (mnt->flags & MS_NODEV)) {
      file_put(f);
      *err = -EACCES;
      return NULL;
    }
  }
  if (f->fops && f->fops->open) {
    int e = f->fops->open(ino, f);
    if (e) {
      f->fops = NULL; /* no release for a failed open */
      file_put(f);
      *err = e;
      return NULL;
    }
  }
  return f;
}

static void set_file_path(struct file *f, int dirfd, const char *path, int flags) {
  char *buf = kmalloc(4096);
  if (buf && path_of(dirfd, path, !(flags & O_NOFOLLOW), buf, 4096) == 0)
    f->path = kstrdup(buf);
  kfree(buf);
}

static int acc_mask(int flags) {
  int acc = flags & O_ACCMODE;
  return acc == O_RDONLY ? MAY_READ : acc == O_WRONLY ? MAY_WRITE : MAY_READ | MAY_WRITE;
}

struct file *open_path(int dirfd, const char *path, int flags, mode_t mode, int *err) {
  struct path p;
  int e;
  struct proc *pr = cur_proc();
  if (flags & O_CREAT) {
    struct path dir;
    const char *last;
    size_t len;
    e = nameiparent_at(dirfd, path, &dir, &last, &len);
    if (e) {
      *err = e;
      return NULL;
    }
    bool trailing = path[strlen(path) - 1] == '/';
    struct inode *i = lookup_one(&dir, last, len, &p);
    if (IS_ERR(i) && PTR_ERR(i) == -ENOENT) {
      if (trailing) {
        path_put(&dir);
        *err = -EISDIR;
        return NULL;
      }
      e = permission(dir.ino, MAY_WRITE | MAY_EXEC);
      if (!e && dir.ino->nlink == 0)
        e = -ENOENT;
      if (!e) {
        i = dir.ino->iops->create(dir.ino, last, len, S_IFREG | (mode & 07777 & ~pr->umask), 0);
        if (IS_ERR(i))
          e = (int)PTR_ERR(i);
        else {
          p.mnt = dir.mnt;
          p.ino = i;
        }
      }
      path_put(&dir);
      if (e) {
        *err = e;
        return NULL;
      }
      /* a new file: no permission check against its mode */
      struct file *f = open_inode(p.ino, p.mnt, flags, err);
      if (f)
        set_file_path(f, dirfd, path, flags);
      path_put(&p);
      return f;
    }
    path_put(&dir);
    if (IS_ERR(i)) {
      *err = (int)PTR_ERR(i);
      return NULL;
    }
    if (flags & O_EXCL) {
      path_put(&p);
      *err = -EEXIST;
      return NULL;
    }
    /* exists: follow a symlink unless O_NOFOLLOW */
    if (S_ISLNK(p.ino->mode)) {
      path_put(&p);
      if (flags & O_NOFOLLOW) {
        *err = -ELOOP;
        return NULL;
      }
      e = namei_at(dirfd, path, LOOKUP_FOLLOW, &p);
      if (e == -ENOENT) {
        /* dangling link: create its target */
        *err = -ENOENT;
        return NULL;
      }
      if (e) {
        *err = e;
        return NULL;
      }
    }
  } else {
    e = namei_at(dirfd, path, (flags & O_NOFOLLOW) ? 0 : LOOKUP_FOLLOW, &p);
    if (e) {
      *err = e;
      return NULL;
    }
    if (S_ISLNK(p.ino->mode) && !(flags & O_PATH)) {
      path_put(&p);
      *err = -ELOOP;
      return NULL;
    }
  }
  struct inode *ino = p.ino;
  if ((flags & O_DIRECTORY) && !S_ISDIR(ino->mode)) {
    path_put(&p);
    *err = -ENOTDIR;
    return NULL;
  }
  if (!(flags & O_PATH)) {
    /* a directory: not for writing, nor with O_CREAT without O_DIRECTORY
       (POSIX.1-2024) */
    if (S_ISDIR(ino->mode) &&
        ((flags & O_ACCMODE) != O_RDONLY || ((flags & O_CREAT) && !(flags & O_DIRECTORY)))) {
      path_put(&p);
      *err = -EISDIR;
      return NULL;
    }
    e = permission(ino, acc_mask(flags) | ((flags & O_TRUNC) && (flags & O_ACCMODE) == O_RDONLY ? MAY_WRITE : 0));
    if (e && !((flags & O_ACCMODE) == 3)) {
      path_put(&p);
      *err = e;
      return NULL;
    }
    if ((flags & O_TRUNC) && S_ISREG(ino->mode) && (flags & O_ACCMODE) != O_RDONLY && ino->size) {
      if (ino->iops->truncate)
        e = ino->iops->truncate(ino, 0);
      if (e) {
        path_put(&p);
        *err = e;
        return NULL;
      }
      inode_set_times(ino, false, true, true);
    }
  }
  struct file *f = open_inode(ino, p.mnt, flags, err);
  if (f)
    set_file_path(f, dirfd, path, flags);
  path_put(&p);
  return f;
}

long do_open(int dirfd, const char *upath, int flags, mode_t mode) {
  char *path = user_path(upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  if (flags & O_TMPFILE) {
    kfree(path);
    return -EOPNOTSUPP;
  }
  int err = 0;
  struct file *f = open_path(dirfd, path, flags, mode, &err);
  kfree(path);
  if (!f)
    return err;
  /* a session leader without a terminal gets this one */
  if (!(flags & O_NOCTTY) && f->ino && S_ISCHR(f->ino->mode) && !(flags & O_PATH))
    tty_maybe_set_ctty(f);
  int fd = fd_install(f, 0, fd_flags_of(flags));
  if (fd < 0)
    file_put(f);
  return fd;
}

long sys_open(long path, long flags, long mode) { return do_open(AT_FDCWD, (const char *)path, (int)flags, (mode_t)mode); }
long sys_openat(long dirfd, long path, long flags, long mode) {
  return do_open((int)dirfd, (const char *)path, (int)flags, (mode_t)mode);
}
long sys_creat(long path, long mode) {
  return do_open(AT_FDCWD, (const char *)path, O_CREAT | O_WRONLY | O_TRUNC, (mode_t)mode);
}

struct open_how {
  uint64_t flags, mode, resolve;
};
long sys_openat2(long dirfd, long path, long uhow, long size) {
  struct open_how h;
  if (size < 24)
    return -EINVAL;
  if (copy_from_user(&h, (void *)uhow, 24))
    return -EFAULT;
  if (h.resolve)
    return -EINVAL;
  return do_open((int)dirfd, (const char *)path, (int)h.flags, (mode_t)h.mode);
}

long sys_close(long fd) {
  int e = fd_close((int)fd);
  return e;
}

long sys_close_range(long first, long last, long flags) {
  struct proc *p = cur_proc();
  if (first > last || (flags & ~6))
    return -EINVAL;
  for (long i = first; i <= last && i < p->nfd; i++) {
    if (!p->fd[i])
      continue;
    if (flags & 4) /* CLOSE_RANGE_CLOEXEC */
      p->fdflags[i] |= FD_CLOEXEC;
    else
      fd_close((int)i);
  }
  return 0;
}

/* ---- read and write ---- */

static ssize_t do_read(struct file *f, char *buf, size_t n, off_t *pos) {
  if ((f->flags & O_ACCMODE) == O_WRONLY || (f->flags & O_PATH))
    return -EBADF;
  if (!f->fops || !f->fops->read)
    return S_ISDIR(f->ino->mode) ? -EISDIR : -EINVAL;
  if (n > 0x7ffff000)
    n = 0x7ffff000;
  return f->fops->read(f, buf, n, pos);
}

static ssize_t do_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  if ((f->flags & O_ACCMODE) == O_RDONLY || (f->flags & O_PATH))
    return -EBADF;
  if (!f->fops || !f->fops->write)
    return -EINVAL;
  if (n > 0x7ffff000)
    n = 0x7ffff000;
  return f->fops->write(f, buf, n, pos);
}

long sys_read(long fd, long buf, long n) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!user_range_ok((uint64_t)buf, (size_t)n))
    return -EFAULT;
  return do_read(f, (char *)buf, (size_t)n, &f->pos);
}

long sys_write(long fd, long buf, long n) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!user_range_ok((uint64_t)buf, (size_t)n))
    return -EFAULT;
  return do_write(f, (const char *)buf, (size_t)n, &f->pos);
}

long sys_pread64(long fd, long buf, long n, long off) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (off < 0)
    return -EINVAL;
  if (!f->fops || !f->fops->seekable)
    return -ESPIPE;
  if (!user_range_ok((uint64_t)buf, (size_t)n))
    return -EFAULT;
  off_t pos = off;
  return do_read(f, (char *)buf, (size_t)n, &pos);
}

static long pwrite_at(struct file *f, const char *buf, size_t n, off_t off, bool noappend) {
  if (off < 0)
    return -EINVAL;
  if (!f->fops || !f->fops->seekable)
    return -ESPIPE;
  off_t pos = off;
  unsigned saved = f->flags;
  if (noappend)
    f->flags &= ~(unsigned)O_APPEND;
  long r = do_write(f, buf, n, &pos);
  f->flags = saved;
  return r;
}

long sys_pwrite64(long fd, long buf, long n, long off) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!user_range_ok((uint64_t)buf, (size_t)n))
    return -EFAULT;
  return pwrite_at(f, (const char *)buf, (size_t)n, off, false);
}

struct iovec {
  uint64_t base, len;
};

static long do_iov(struct file *f, long uiov, long cnt, off_t *pos, bool write) {
  if (cnt < 0 || cnt > 1024)
    return -EINVAL;
  long total = 0;
  for (long i = 0; i < cnt; i++) {
    struct iovec v;
    if (copy_from_user(&v, (struct iovec *)uiov + i, sizeof(v)))
      return -EFAULT;
    if ((int64_t)v.len < 0)
      return -EINVAL;
    if (!user_range_ok(v.base, v.len))
      return -EFAULT;
  }
  for (long i = 0; i < cnt; i++) {
    struct iovec v;
    copy_from_user(&v, (struct iovec *)uiov + i, sizeof(v));
    if (!v.len)
      continue;
    ssize_t r = write ? do_write(f, (const char *)v.base, v.len, pos) : do_read(f, (char *)v.base, v.len, pos);
    if (r < 0)
      return total ? total : r;
    total += r;
    if ((size_t)r < v.len)
      break;
  }
  return total;
}

long sys_readv(long fd, long iov, long cnt) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if ((f->flags & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  return do_iov(f, iov, cnt, &f->pos, false);
}

long sys_writev(long fd, long iov, long cnt) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if ((f->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;
  return do_iov(f, iov, cnt, &f->pos, true);
}

long sys_preadv(long fd, long iov, long cnt, long off) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!f->fops || !f->fops->seekable)
    return -ESPIPE;
  off_t pos = off;
  return do_iov(f, iov, cnt, &pos, false);
}

long sys_pwritev(long fd, long iov, long cnt, long off) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!f->fops || !f->fops->seekable)
    return -ESPIPE;
  off_t pos = off;
  return do_iov(f, iov, cnt, &pos, true);
}

#define RWF_NOAPPEND 0x20

long sys_pwritev2(long fd, long iov, long cnt, long off, long offhi, long flags) {
  (void)offhi;
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (flags & ~(0x3fL))
    return -EOPNOTSUPP;
  if (off == -1)
    return do_iov(f, iov, cnt, &f->pos, true);
  if (!f->fops || !f->fops->seekable)
    return -ESPIPE;
  off_t pos = off;
  unsigned saved = f->flags;
  if (flags & RWF_NOAPPEND)
    f->flags &= ~(unsigned)O_APPEND;
  long r = do_iov(f, iov, cnt, &pos, true);
  f->flags = saved;
  return r;
}

long sys_preadv2(long fd, long iov, long cnt, long off, long offhi, long flags) {
  (void)offhi;
  (void)flags;
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (off == -1)
    return do_iov(f, iov, cnt, &f->pos, false);
  return sys_preadv(fd, iov, cnt, off);
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_DATA 3
#define SEEK_HOLE 4

long sys_lseek(long fd, long off, long whence) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!f->fops || !f->fops->seekable) {
    if (f->ino && S_ISDIR(f->ino->mode) && whence == SEEK_SET && off == 0) {
      f->pos = 0;
      return 0;
    }
    return -ESPIPE;
  }
  off_t n;
  switch (whence) {
  case SEEK_SET:
    n = off;
    break;
  case SEEK_CUR:
    n = f->pos + off;
    break;
  case SEEK_END:
    n = (off_t)f->ino->size + off;
    break;
  case SEEK_DATA:
    if (off >= (off_t)f->ino->size)
      return -ENXIO;
    n = off;
    break;
  case SEEK_HOLE:
    if (off >= (off_t)f->ino->size)
      return -ENXIO;
    n = (off_t)f->ino->size;
    break;
  default:
    return -EINVAL;
  }
  if (n < 0)
    return -EINVAL;
  f->pos = n;
  return n;
}

/* ---- dup, fcntl ---- */

long sys_dup(long fd) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  file_get(f);
  int r = fd_install(f, 0, 0);
  if (r < 0)
    file_put(f);
  return r;
}

static long do_dup3(long oldfd, long newfd, long flags) {
  struct proc *p = cur_proc();
  struct file *f = fd_get((int)oldfd);
  if (!f)
    return -EBADF;
  if (newfd < 0 || (uint64_t)newfd >= MIN(p->rlim[RLIMIT_NOFILE].cur, (uint64_t)MAX_FD))
    return -EBADF;
  if (oldfd == newfd)
    return -EINVAL;
  if (newfd >= p->nfd && fd_grow(p, (int)newfd + 1))
    return -ENOMEM;
  file_get(f);
  if (p->fd[newfd])
    fd_close((int)newfd);
  p->fd[newfd] = f;
  p->fdflags[newfd] = (uint8_t)fd_flags_of(flags);
  return newfd;
}

long sys_dup3(long oldfd, long newfd, long flags) {
  if (flags & ~(O_CLOEXEC | O_CLOFORK))
    return -EINVAL;
  return do_dup3(oldfd, newfd, flags);
}

long sys_dup2(long oldfd, long newfd) {
  if (oldfd == newfd)
    return fd_get((int)oldfd) ? newfd : -EBADF;
  return do_dup3(oldfd, newfd, 0);
}

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_DUPFD_CLOEXEC 1030
#define F_GETPIPE_SZ 1032
#define F_DUPFD_CLOFORK 0x10000 /* the value of Shall's C library */
#define F_OFD_GETLK 36
#define F_OFD_SETLK 37
#define F_OFD_SETLKW 38

long sys_fcntl(long fd, long cmd, long arg) {
  struct proc *p = cur_proc();
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  switch (cmd) {
  case F_DUPFD:
  case F_DUPFD_CLOEXEC:
  case F_DUPFD_CLOFORK: {
    if (arg < 0 || (uint64_t)arg >= p->rlim[RLIMIT_NOFILE].cur)
      return -EINVAL;
    file_get(f);
    int r = fd_install(f, (int)arg, cmd == F_DUPFD_CLOEXEC ? FD_CLOEXEC : cmd == F_DUPFD_CLOFORK ? FD_CLOFORK : 0);
    if (r < 0)
      file_put(f);
    return r;
  }
  case F_GETFD:
    return p->fdflags[fd] & (FD_CLOEXEC | FD_CLOFORK);
  case F_SETFD:
    p->fdflags[fd] = (uint8_t)(arg & (FD_CLOEXEC | FD_CLOFORK));
    return 0;
  case F_GETFL:
    return f->flags | (f->flags & O_PATH ? 0 : 0);
  case F_SETFL: {
    unsigned ch = O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT | O_NOATIME;
    f->flags = (f->flags & ~ch) | ((unsigned)arg & ch);
    return 0;
  }
  case F_GETLK:
  case F_SETLK:
  case F_SETLKW:
  case F_OFD_GETLK:
  case F_OFD_SETLK:
  case F_OFD_SETLKW:
    return do_fcntl_lock(f, (int)cmd, (void *)arg);
  case F_GETOWN:
    return 0;
  case F_SETOWN:
    return 0;
  case F_GETPIPE_SZ:
    return f->ino && f->ino->pipe ? 65536 : -EBADF;
  }
  return -EINVAL;
}

/* ---- ioctl ---- */

#define FIONREAD 0x541b
#define FIONBIO 0x5421
#define FIOCLEX 0x5451
#define FIONCLEX 0x5450

long do_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  if (f->fops && f->fops->ioctl)
    return f->fops->ioctl(f, cmd, arg);
  if (cmd == FIONREAD && f->ino && S_ISREG(f->ino->mode)) {
    int32_t n = f->pos < (off_t)f->ino->size ? (int32_t)(f->ino->size - f->pos) : 0;
    return copy_to_user((void *)arg, &n, 4) ? -EFAULT : 0;
  }
  return -ENOTTY;
}

long sys_ioctl(long fd, long cmd, long arg) {
  struct proc *p = cur_proc();
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  switch ((unsigned)cmd) {
  case FIOCLEX:
    p->fdflags[fd] |= FD_CLOEXEC;
    return 0;
  case FIONCLEX:
    p->fdflags[fd] &= ~FD_CLOEXEC;
    return 0;
  case FIONBIO: {
    int32_t on;
    if (copy_from_user(&on, (void *)arg, 4))
      return -EFAULT;
    if (on)
      f->flags |= O_NONBLOCK;
    else
      f->flags &= ~(unsigned)O_NONBLOCK;
    return 0;
  }
  }
  if (f->flags & O_PATH)
    return -EBADF;
  return do_ioctl(f, (unsigned)cmd, (uint64_t)arg);
}

/* ---- poll and select ---- */

static unsigned file_poll(struct file *f) {
  if (f->fops && f->fops->poll)
    return f->fops->poll(f);
  return POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
}

struct pollfd {
  int32_t fd;
  int16_t events, revents;
};

/* deadline: monotonic ns, 0 = forever, 1 = do not wait */
static long do_poll(struct pollfd *fds, unsigned n, uint64_t deadline) {
  uint64_t f = irq_save();
  for (;;) {
    int ready = 0;
    for (unsigned i = 0; i < n; i++) {
      fds[i].revents = 0;
      if (fds[i].fd < 0)
        continue;
      struct file *fl = fd_get(fds[i].fd);
      if (!fl) {
        fds[i].revents = POLLNVAL;
        ready++;
        continue;
      }
      unsigned m = file_poll(fl);
      fds[i].revents = (int16_t)(m & ((unsigned)(uint16_t)fds[i].events | POLLERR | POLLHUP | POLLNVAL));
      if (fds[i].revents)
        ready++;
    }
    if (ready || deadline == 1) {
      irq_restore(f);
      return ready;
    }
    int e = sleep_on(&poll_wait, deadline, true);
    if (e == -ETIMEDOUT) {
      irq_restore(f);
      return 0;
    }
    if (e == -EINTR) {
      irq_restore(f);
      return -ERESTARTNOHAND;
    }
  }
}

static uint64_t ts_deadline(long uts, int *err) {
  *err = 0;
  if (!uts)
    return 0;
  struct timespec64 ts;
  if (copy_from_user(&ts, (void *)uts, sizeof(ts))) {
    *err = -EFAULT;
    return 0;
  }
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= 1000000000) {
    *err = -EINVAL;
    return 0;
  }
  if (ts.sec == 0 && ts.nsec == 0)
    return 1;
  return sat_add(monotonic_ns(), ts_to_ns(ts.sec, ts.nsec));
}

static long poll_common(long ufds, long n, uint64_t deadline) {
  if (n < 0 || (uint64_t)n > cur_proc()->rlim[RLIMIT_NOFILE].cur)
    return -EINVAL;
  struct pollfd *fds = kmalloc((size_t)n * sizeof(*fds) + 1);
  if (!fds)
    return -ENOMEM;
  if (copy_from_user(fds, (void *)ufds, (size_t)n * sizeof(*fds))) {
    kfree(fds);
    return -EFAULT;
  }
  long r = do_poll(fds, (unsigned)n, deadline);
  if (r >= 0)
    for (long i = 0; i < n; i++)
      if (copy_to_user(&((struct pollfd *)ufds)[i].revents, &fds[i].revents, 2)) {
        r = -EFAULT;
        break;
      }
  kfree(fds);
  return r;
}

long sys_poll(long ufds, long n, long timeout) {
  uint64_t d = timeout < 0 ? 0 : timeout == 0 ? 1 : sat_add(monotonic_ns(), (uint64_t)timeout * 1000000UL);
  long r = poll_common(ufds, n, d);
  return r == -ERESTARTNOHAND ? -EINTR : r;
}

static void set_mask_for_wait(long umask, long size, int *err) {
  *err = 0;
  if (!umask)
    return;
  if (size != 8) {
    *err = -EINVAL;
    return;
  }
  sigset_t s;
  if (copy_from_user(&s, (void *)umask, 8)) {
    *err = -EFAULT;
    return;
  }
  current->saved_mask = current->blocked;
  current->restore_mask = true;
  current->blocked = s & ~UNBLOCKABLE;
}

long sys_ppoll(long ufds, long n, long uts, long umask, long size) {
  int err;
  uint64_t d = ts_deadline(uts, &err);
  if (err)
    return err;
  set_mask_for_wait(umask, size, &err);
  if (err)
    return err;
  long r = poll_common(ufds, n, d);
  if (r == -ERESTARTNOHAND)
    return -EINTR; /* the saved mask is restored after the handler */
  if (umask && !signal_pending(current)) {
    current->blocked = current->saved_mask;
    current->restore_mask = false;
  }
  return r;
}

static long do_select(int n, uint64_t *rin, uint64_t *win, uint64_t *ein, uint64_t deadline) {
  int words = (n + 63) / 64;
  uint64_t r[16], w[16], e[16];
  uint64_t f = irq_save();
  for (;;) {
    int ready = 0;
    for (int i = 0; i < words; i++)
      r[i] = w[i] = e[i] = 0;
    for (int fd = 0; fd < n; fd++) {
      uint64_t bit = 1UL << (fd & 63);
      int k = fd / 64;
      bool want_r = rin && (rin[k] & bit), want_w = win && (win[k] & bit), want_e = ein && (ein[k] & bit);
      if (!want_r && !want_w && !want_e)
        continue;
      struct file *fl = fd_get(fd);
      if (!fl) {
        irq_restore(f);
        return -EBADF;
      }
      unsigned m = file_poll(fl);
      if (want_r && (m & (POLLIN | POLLRDNORM | POLLHUP | POLLERR)))
        r[k] |= bit, ready++;
      if (want_w && (m & (POLLOUT | POLLWRNORM | POLLERR)))
        w[k] |= bit, ready++;
      if (want_e && (m & POLLPRI))
        e[k] |= bit, ready++;
    }
    if (ready || deadline == 1) {
      for (int i = 0; i < words; i++) {
        if (rin)
          rin[i] = r[i];
        if (win)
          win[i] = w[i];
        if (ein)
          ein[i] = e[i];
      }
      irq_restore(f);
      return ready;
    }
    int s = sleep_on(&poll_wait, deadline, true);
    if (s == -ETIMEDOUT) {
      for (int i = 0; i < words; i++) {
        if (rin)
          rin[i] = 0;
        if (win)
          win[i] = 0;
        if (ein)
          ein[i] = 0;
      }
      irq_restore(f);
      return 0;
    }
    if (s == -EINTR) {
      irq_restore(f);
      return -ERESTARTNOHAND;
    }
  }
}

static long select_common(long n, long ur, long uw, long ue, uint64_t deadline) {
  if (n < 0)
    return -EINVAL;
  if (n > 1024)
    n = 1024;
  size_t bytes = (size_t)((n + 63) / 64) * 8;
  uint64_t r[16], w[16], e[16];
  if ((ur && copy_from_user(r, (void *)ur, bytes)) || (uw && copy_from_user(w, (void *)uw, bytes)) ||
      (ue && copy_from_user(e, (void *)ue, bytes)))
    return -EFAULT;
  long res = do_select((int)n, ur ? r : NULL, uw ? w : NULL, ue ? e : NULL, deadline);
  if (res >= 0) {
    if ((ur && copy_to_user((void *)ur, r, bytes)) || (uw && copy_to_user((void *)uw, w, bytes)) ||
        (ue && copy_to_user((void *)ue, e, bytes)))
      return -EFAULT;
  }
  return res;
}

long sys_select(long n, long ur, long uw, long ue, long utv) {
  uint64_t d = 0;
  if (utv) {
    int64_t tv[2];
    if (copy_from_user(tv, (void *)utv, sizeof(tv)))
      return -EFAULT;
    if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000)
      return -EINVAL;
    d = (tv[0] == 0 && tv[1] == 0) ? 1 : sat_add(monotonic_ns(), sat_add(ts_to_ns(tv[0], 0), (uint64_t)tv[1] * 1000));
  }
  long r = select_common(n, ur, uw, ue, d);
  return r == -ERESTARTNOHAND ? -EINTR : r;
}

long sys_pselect6(long n, long ur, long uw, long ue, long uts, long usig) {
  int err;
  uint64_t d = ts_deadline(uts, &err);
  if (err)
    return err;
  if (usig) {
    uint64_t sg[2];
    if (copy_from_user(sg, (void *)usig, sizeof(sg)))
      return -EFAULT;
    if (sg[0]) {
      set_mask_for_wait((long)sg[0], (long)sg[1], &err);
      if (err)
        return err;
    }
  }
  long r = select_common(n, ur, uw, ue, d);
  if (r == -ERESTARTNOHAND)
    return -EINTR;
  if (current->restore_mask && !signal_pending(current)) {
    current->blocked = current->saved_mask;
    current->restore_mask = false;
  }
  return r;
}

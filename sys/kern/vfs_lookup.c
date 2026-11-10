/* namei.c - system calls on names: stat, mkdir, mknod, unlink, rmdir,
   rename, link, symlink, readlink, chmod, chown, utimensat, truncate,
   access, chdir, getcwd, getdents64, statfs, sync. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>

/* struct stat of Linux x86-64: 144 bytes */
struct kstat {
  uint64_t dev, ino, nlink;
  uint32_t mode, uid, gid, pad0;
  uint64_t rdev;
  int64_t size, blksize, blocks;
  int64_t atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns;
  int64_t unused[3];
};

static void make_stat(struct inode *i, struct kstat *st) {
  memset(st, 0, sizeof(*st));
  st->dev = i->sb->dev;
  st->ino = i->ino;
  st->nlink = i->nlink;
  st->mode = i->mode;
  st->uid = i->uid;
  st->gid = i->gid;
  st->rdev = i->rdev;
  st->size = (int64_t)i->size;
  st->blksize = i->sb->blocksize ? i->sb->blocksize : 4096;
  st->blocks = (int64_t)i->blocks;
  st->atime = i->atime.sec;
  st->atime_ns = i->atime.nsec;
  st->mtime = i->mtime.sec;
  st->mtime_ns = i->mtime.nsec;
  st->ctime = i->ctime.sec;
  st->ctime_ns = i->ctime.nsec;
}

int fill_stat(struct inode *i, void *ustat) {
  struct kstat st;
  make_stat(i, &st);
  return copy_to_user(ustat, &st, sizeof(st)) ? -EFAULT : 0;
}

static long stat_at(int dirfd, const char *upath, void *ust, int flags) {
  if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH | AT_NO_AUTOMOUNT))
    return -EINVAL;
  char *path = user_path(upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path p;
  int e;
  if (!*path && (flags & AT_EMPTY_PATH)) {
    if (dirfd == AT_FDCWD) {
      p = cur_proc()->cwd;
      path_get(&p);
      e = 0;
    } else {
      struct file *f = fd_get(dirfd);
      kfree(path);
      if (!f)
        return -EBADF;
      return fill_stat(f->ino, ust);
    }
  } else
    e = namei_at(dirfd, path, (flags & AT_SYMLINK_NOFOLLOW) ? 0 : LOOKUP_FOLLOW, &p);
  kfree(path);
  if (e)
    return e;
  e = fill_stat(p.ino, ust);
  path_put(&p);
  return e;
}

long sys_stat(long path, long ust) { return stat_at(AT_FDCWD, (const char *)path, (void *)ust, 0); }
long sys_lstat(long path, long ust) {
  return stat_at(AT_FDCWD, (const char *)path, (void *)ust, AT_SYMLINK_NOFOLLOW);
}
long sys_newfstatat(long dirfd, long path, long ust, long flags) {
  return stat_at((int)dirfd, (const char *)path, (void *)ust, (int)flags);
}
long sys_fstat(long fd, long ust) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!f->ino)
    return -EBADF;
  return fill_stat(f->ino, (void *)ust);
}

/* statx (some C libraries use it) */
struct kstatx {
  uint32_t mask, blksize;
  uint64_t attributes;
  uint32_t nlink, uid, gid;
  uint16_t mode, pad1;
  uint64_t ino, size, blocks, attributes_mask;
  struct {
    int64_t sec;
    uint32_t nsec;
    int32_t pad;
  } atime, btime, ctime, mtime;
  uint32_t rdev_major, rdev_minor, dev_major, dev_minor;
  uint64_t spare[14];
};

long sys_statx(long dirfd, long upath, long flags, long mask, long ubuf) {
  (void)mask;
  struct kstat st;
  /* reuse stat_at through a kernel buffer */
  long e = stat_at((int)dirfd, (const char *)upath, &st, (int)flags & (AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH));
  if (e)
    return e;
  struct kstatx x;
  memset(&x, 0, sizeof(x));
  x.mask = 0x7ff;
  x.blksize = (uint32_t)st.blksize;
  x.nlink = (uint32_t)st.nlink;
  x.uid = st.uid;
  x.gid = st.gid;
  x.mode = (uint16_t)st.mode;
  x.ino = st.ino;
  x.size = (uint64_t)st.size;
  x.blocks = (uint64_t)st.blocks;
  x.atime.sec = st.atime, x.atime.nsec = (uint32_t)st.atime_ns;
  x.mtime.sec = st.mtime, x.mtime.nsec = (uint32_t)st.mtime_ns;
  x.ctime.sec = st.ctime, x.ctime.nsec = (uint32_t)st.ctime_ns;
  x.rdev_major = MAJOR(st.rdev), x.rdev_minor = MINOR(st.rdev);
  x.dev_major = MAJOR(st.dev), x.dev_minor = MINOR(st.dev);
  return copy_to_user((void *)ubuf, &x, sizeof(x)) ? -EFAULT : 0;
}

/* ---- creating ---- */

static bool may_write_dir(struct inode *dir, int *e) {
  *e = permission(dir, MAY_WRITE | MAY_EXEC);
  return *e == 0;
}

static long mknod_at(int dirfd, const char *upath, mode_t mode, dev_t dev) {
  char *path = user_path(upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path dir, ex;
  const char *last;
  size_t len;
  int e = nameiparent_at(dirfd, path, &dir, &last, &len);
  if (e) {
    kfree(path);
    return e;
  }
  struct inode *i = lookup_one(&dir, last, len, &ex);
  if (!IS_ERR(i)) {
    path_put(&ex);
    e = -EEXIST;
  } else if (PTR_ERR(i) != -ENOENT)
    e = (int)PTR_ERR(i);
  else if (may_write_dir(dir.ino, &e)) {
    if (dir.ino->nlink == 0)
      e = -ENOENT;
    else {
      mode_t m = (mode & S_IFMT) | (mode & 07777 & ~cur_proc()->umask);
      i = dir.ino->iops->create(dir.ino, last, len, m, dev);
      if (IS_ERR(i))
        e = (int)PTR_ERR(i);
      else
        iput(i);
    }
  }
  path_put(&dir);
  kfree(path);
  return e;
}

long sys_mknodat(long dirfd, long path, long mode, long dev) {
  mode_t m = (mode_t)mode;
  if ((m & S_IFMT) == 0)
    m |= S_IFREG;
  if (S_ISDIR(m) || S_ISLNK(m))
    return -EINVAL;
  if ((S_ISCHR(m) || S_ISBLK(m)) && !capable())
    return -EPERM;
  return mknod_at((int)dirfd, (const char *)path, m, (dev_t)dev);
}
long sys_mknod(long path, long mode, long dev) { return sys_mknodat(AT_FDCWD, path, mode, dev); }

long sys_mkdirat(long dirfd, long path, long mode) {
  return mknod_at((int)dirfd, (const char *)path, S_IFDIR | ((mode_t)mode & 07777), 0);
}
long sys_mkdir(long path, long mode) { return sys_mkdirat(AT_FDCWD, path, mode); }

long sys_symlinkat(long utarget, long dirfd, long upath) {
  char *target = user_path((const char *)utarget);
  if (IS_ERR(target))
    return PTR_ERR(target);
  char *path = user_path((const char *)upath);
  if (IS_ERR(path)) {
    kfree(target);
    return PTR_ERR(path);
  }
  struct path dir, ex;
  const char *last;
  size_t len;
  int e = *target ? nameiparent_at((int)dirfd, path, &dir, &last, &len) : -ENOENT;
  if (!e) {
    struct inode *i = lookup_one(&dir, last, len, &ex);
    if (!IS_ERR(i)) {
      path_put(&ex);
      e = -EEXIST;
    } else if (PTR_ERR(i) != -ENOENT)
      e = (int)PTR_ERR(i);
    else if (may_write_dir(dir.ino, &e)) {
      i = dir.ino->iops->symlink(dir.ino, last, len, target);
      if (IS_ERR(i))
        e = (int)PTR_ERR(i);
      else
        iput(i);
    }
    path_put(&dir);
  }
  kfree(path);
  kfree(target);
  return e;
}
long sys_symlink(long target, long path) { return sys_symlinkat(target, AT_FDCWD, path); }

long sys_linkat(long odirfd, long uold, long ndirfd, long unew, long flags) {
  if (flags & ~(AT_SYMLINK_FOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  char *old = user_path((const char *)uold);
  if (IS_ERR(old))
    return PTR_ERR(old);
  char *nw = user_path((const char *)unew);
  if (IS_ERR(nw)) {
    kfree(old);
    return PTR_ERR(nw);
  }
  struct path src, dir, ex;
  const char *last;
  size_t len;
  int e = namei_at((int)odirfd, old, (flags & AT_SYMLINK_FOLLOW) ? LOOKUP_FOLLOW : 0, &src);
  if (!e) {
    e = nameiparent_at((int)ndirfd, nw, &dir, &last, &len);
    if (!e) {
      struct inode *i = lookup_one(&dir, last, len, &ex);
      if (!IS_ERR(i)) {
        path_put(&ex);
        e = -EEXIST;
      } else if (PTR_ERR(i) != -ENOENT)
        e = (int)PTR_ERR(i);
      else if (dir.mnt->sb != src.mnt->sb)
        e = -EXDEV;
      else if (S_ISDIR(src.ino->mode))
        e = -EPERM;
      else if (may_write_dir(dir.ino, &e)) {
        if (!dir.ino->iops->link)
          e = -EPERM;
        else if (src.ino->nlink >= 65000)
          e = -EMLINK;
        else
          e = dir.ino->iops->link(dir.ino, last, len, src.ino);
        if (!e)
          inode_set_times(src.ino, false, false, true);
      }
      path_put(&dir);
    }
    path_put(&src);
  }
  kfree(old);
  kfree(nw);
  return e;
}
long sys_link(long o, long n) { return sys_linkat(AT_FDCWD, o, AT_FDCWD, n, 0); }

/* sticky directories: only the owner of the file or the directory */
static int sticky_ok(struct inode *dir, struct inode *i) {
  if (!(dir->mode & S_ISVTX))
    return 0;
  struct cred *c = &cur_proc()->cred;
  if (c->fsuid == 0 || c->fsuid == i->uid || c->fsuid == dir->uid)
    return 0;
  return -EPERM;
}

static bool is_mountpoint(struct inode *i) {
  if (i->mounted)
    return true;
  list_for_each(p, &mounts) {
    if (list_entry(p, struct mount, link)->root == i)
      return true;
  }
  return false;
}

long sys_unlinkat(long dirfd, long upath, long flags) {
  if (flags & ~AT_REMOVEDIR)
    return -EINVAL;
  char *path = user_path((const char *)upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path dir, t;
  const char *last;
  size_t len;
  int e = nameiparent_at((int)dirfd, path, &dir, &last, &len);
  if (e) {
    kfree(path);
    return e;
  }
  bool rmdir = flags & AT_REMOVEDIR;
  if ((len == 1 && last[0] == '.') || (len == 2 && last[0] == '.' && last[1] == '.')) {
    e = rmdir ? (len == 1 ? -EINVAL : -ENOTEMPTY) : -EISDIR;
    path_put(&dir);
    kfree(path);
    return e;
  }
  struct inode *i = lookup_one(&dir, last, len, &t);
  if (IS_ERR(i))
    e = (int)PTR_ERR(i);
  else {
    if (t.mnt != dir.mnt || is_mountpoint(i))
      e = -EBUSY;
    else if (!may_write_dir(dir.ino, &e))
      ;
    else if ((e = sticky_ok(dir.ino, i)))
      ;
    else if (rmdir) {
      if (!S_ISDIR(i->mode))
        e = -ENOTDIR;
      else
        e = dir.ino->iops->rmdir(dir.ino, last, len, i);
    } else {
      if (S_ISDIR(i->mode))
        e = -EISDIR;
      else if (path[strlen(path) - 1] == '/')
        e = -ENOTDIR;
      else
        e = dir.ino->iops->unlink(dir.ino, last, len, i);
    }
    path_put(&t);
  }
  path_put(&dir);
  kfree(path);
  return e;
}
long sys_unlink(long path) { return sys_unlinkat(AT_FDCWD, path, 0); }
long sys_rmdir(long path) { return sys_unlinkat(AT_FDCWD, path, AT_REMOVEDIR); }

/* is a an ancestor of (or equal to) b? */
static bool is_ancestor(struct path *a, struct path *b) {
  struct path cur = *b;
  path_get(&cur);
  bool r = false;
  for (int n = 0; n < 4096; n++) {
    if (cur.ino == a->ino) {
      r = true;
      break;
    }
    if (cur.ino == cur.mnt->root)
      break;
    struct path up;
    struct inode *u = lookup_one(&cur, "..", 2, &up);
    if (IS_ERR(u))
      break;
    if (up.ino == cur.ino) {
      path_put(&up);
      break;
    }
    path_put(&cur);
    cur = up;
  }
  path_put(&cur);
  return r;
}

#define RENAME_NOREPLACE 1
#define RENAME_EXCHANGE 2

long sys_renameat2(long odirfd, long uold, long ndirfd, long unew, long flags) {
  if (flags & ~RENAME_NOREPLACE)
    return flags & RENAME_EXCHANGE ? -EOPNOTSUPP : -EINVAL;
  char *old = user_path((const char *)uold);
  if (IS_ERR(old))
    return PTR_ERR(old);
  char *nw = user_path((const char *)unew);
  if (IS_ERR(nw)) {
    kfree(old);
    return PTR_ERR(nw);
  }
  struct path od, nd, src, dst;
  const char *ol, *nl;
  size_t olen, nlen;
  int e = nameiparent_at((int)odirfd, old, &od, &ol, &olen);
  if (e)
    goto out0;
  e = nameiparent_at((int)ndirfd, nw, &nd, &nl, &nlen);
  if (e)
    goto out1;
  if ((olen == 1 && ol[0] == '.') || (olen == 2 && ol[0] == '.' && ol[1] == '.') ||
      (nlen == 1 && nl[0] == '.') || (nlen == 2 && nl[0] == '.' && nl[1] == '.')) {
    e = -EINVAL;
    if ((olen <= 2 && ol[0] == '.') && (nlen <= 2 && nl[0] == '.'))
      e = -EBUSY;
    goto out2;
  }
  struct inode *si = lookup_one(&od, ol, olen, &src);
  if (IS_ERR(si)) {
    e = (int)PTR_ERR(si);
    goto out2;
  }
  if (od.mnt->sb != nd.mnt->sb || src.mnt != od.mnt) {
    e = src.mnt != od.mnt ? -EBUSY : -EXDEV;
    goto out3;
  }
  if (is_mountpoint(si)) {
    e = -EBUSY;
    goto out3;
  }
  struct inode *ti = lookup_one(&nd, nl, nlen, &dst);
  if (IS_ERR(ti)) {
    if (PTR_ERR(ti) != -ENOENT) {
      e = (int)PTR_ERR(ti);
      goto out3;
    }
    ti = NULL;
  }
  if (ti && (flags & RENAME_NOREPLACE)) {
    e = -EEXIST;
    goto out4;
  }
  if (ti == si) {
    e = 0; /* the same file: nothing to do */
    goto out4;
  }
  bool sdir = S_ISDIR(si->mode);
  if (ti) {
    if (is_mountpoint(ti)) {
      e = -EBUSY;
      goto out4;
    }
    if (sdir && !S_ISDIR(ti->mode)) {
      e = -ENOTDIR;
      goto out4;
    }
    if (!sdir && S_ISDIR(ti->mode)) {
      e = -EISDIR;
      goto out4;
    }
  }
  if (!sdir && (old[strlen(old) - 1] == '/' || nw[strlen(nw) - 1] == '/')) {
    e = -ENOTDIR;
    goto out4;
  }
  if (!may_write_dir(od.ino, &e) || !may_write_dir(nd.ino, &e))
    goto out4;
  if ((e = sticky_ok(od.ino, si)) || (ti && (e = sticky_ok(nd.ino, ti))))
    goto out4;
  if (sdir && od.ino != nd.ino && (e = permission(si, MAY_WRITE)))
    goto out4;
  /* a directory cannot move below itself */
  if (sdir && is_ancestor(&src, &nd)) {
    e = -EINVAL;
    goto out4;
  }
  e = od.ino->iops->rename(od.ino, ol, olen, si, nd.ino, nl, nlen, ti);
out4:
  if (ti)
    path_put(&dst);
out3:
  path_put(&src);
out2:
  path_put(&nd);
out1:
  path_put(&od);
out0:
  kfree(old);
  kfree(nw);
  return e;
}
long sys_renameat(long od, long o, long nd, long n) { return sys_renameat2(od, o, nd, n, 0); }
long sys_rename(long o, long n) { return sys_renameat2(AT_FDCWD, o, AT_FDCWD, n, 0); }

long sys_readlinkat(long dirfd, long upath, long ubuf, long n) {
  if (n <= 0)
    return -EINVAL;
  char *path = user_path((const char *)upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path p;
  int e = namei_at((int)dirfd, path, LOOKUP_EMPTY, &p);
  if (!*path && !e) {
    path_put(&p);
    struct file *f = fd_get((int)dirfd);
    e = f ? 0 : -EBADF;
    if (f) {
      p.mnt = f->mnt;
      p.ino = igrab(f->ino);
    }
  }
  kfree(path);
  if (e)
    return e;
  long r;
  if (!S_ISLNK(p.ino->mode) || !p.ino->iops->readlink)
    r = -EINVAL;
  else {
    char *buf = kmalloc(4096);
    r = p.ino->iops->readlink(p.ino, buf, 4096);
    if (r > 0) {
      if (r > n)
        r = n;
      if (copy_to_user((void *)ubuf, buf, (size_t)r))
        r = -EFAULT;
    }
    kfree(buf);
  }
  path_put(&p);
  return r;
}
long sys_readlink(long path, long buf, long n) { return sys_readlinkat(AT_FDCWD, path, buf, n); }

/* ---- attributes ---- */

static int do_chmod(struct inode *i, mode_t mode) {
  if (S_ISLNK(i->mode))
    return -EOPNOTSUPP; /* as Linux: symbolic links have no mode to change */
  if (!owner_or_root(i))
    return -EPERM;
  if (i->sb->rdonly)
    return -EROFS;
  struct cred *c = &cur_proc()->cred;
  if (c->fsuid != 0 && !in_group(i->gid))
    mode &= ~S_ISGID;
  i->mode = (i->mode & S_IFMT) | (mode & 07777);
  inode_set_times(i, false, false, true);
  return 0;
}

static long path_op(int dirfd, const char *upath, int lflags, int (*fn)(struct inode *, void *), void *arg) {
  char *path = user_path(upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path p;
  int e;
  if (!*path && (lflags & LOOKUP_EMPTY)) {
    kfree(path);
    if (dirfd == AT_FDCWD) {
      p = cur_proc()->cwd;
      return fn(p.ino, arg);
    }
    struct file *f = fd_get(dirfd);
    if (!f)
      return -EBADF;
    return fn(f->ino, arg);
  }
  e = namei_at(dirfd, path, lflags & ~LOOKUP_EMPTY, &p);
  kfree(path);
  if (e)
    return e;
  e = fn(p.ino, arg);
  path_put(&p);
  return e;
}

static int chmod_fn(struct inode *i, void *arg) { return do_chmod(i, *(mode_t *)arg); }

long sys_fchmodat2(long dirfd, long path, long mode, long flags) {
  if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  mode_t m = (mode_t)mode;
  int lf = ((flags & AT_SYMLINK_NOFOLLOW) ? 0 : LOOKUP_FOLLOW) | ((flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0);
  return path_op((int)dirfd, (const char *)path, lf, chmod_fn, &m);
}
long sys_fchmodat(long dirfd, long path, long mode) { return sys_fchmodat2(dirfd, path, mode, 0); }
long sys_chmod(long path, long mode) { return sys_fchmodat2(AT_FDCWD, path, mode, 0); }
long sys_fchmod(long fd, long mode) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  return do_chmod(f->ino, (mode_t)mode);
}

struct owner {
  uint32_t uid, gid;
};

static int chown_fn(struct inode *i, void *arg) {
  struct owner *o = arg;
  struct cred *c = &cur_proc()->cred;
  if (i->sb->rdonly)
    return -EROFS;
  if (c->fsuid != 0) {
    if (o->uid != (uint32_t)-1 && o->uid != i->uid)
      return -EPERM;
    if (c->fsuid != i->uid)
      return o->uid == (uint32_t)-1 && o->gid == (uint32_t)-1 ? -EPERM : -EPERM;
    if (o->gid != (uint32_t)-1 && o->gid != i->gid && !in_group(o->gid))
      return -EPERM;
  }
  if (o->uid != (uint32_t)-1)
    i->uid = o->uid;
  if (o->gid != (uint32_t)-1)
    i->gid = o->gid;
  if (!S_ISDIR(i->mode) && (o->uid != (uint32_t)-1 || o->gid != (uint32_t)-1)) {
    /* as Linux: clear set-user-ID, and set-group-ID if group-executable */
    i->mode &= ~S_ISUID;
    if (i->mode & 010)
      i->mode &= ~S_ISGID;
  }
  inode_set_times(i, false, false, true);
  return 0;
}

long sys_fchownat(long dirfd, long path, long uid, long gid, long flags) {
  if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  struct owner o = {(uint32_t)uid, (uint32_t)gid};
  int lf = ((flags & AT_SYMLINK_NOFOLLOW) ? 0 : LOOKUP_FOLLOW) | ((flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0);
  return path_op((int)dirfd, (const char *)path, lf, chown_fn, &o);
}
long sys_chown(long p, long u, long g) { return sys_fchownat(AT_FDCWD, p, u, g, 0); }
long sys_lchown(long p, long u, long g) { return sys_fchownat(AT_FDCWD, p, u, g, AT_SYMLINK_NOFOLLOW); }
long sys_fchown(long fd, long uid, long gid) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  struct owner o = {(uint32_t)uid, (uint32_t)gid};
  return chown_fn(f->ino, &o);
}

#define UTIME_NOW ((1L << 30) - 1)
#define UTIME_OMIT ((1L << 30) - 2)

struct times_arg {
  struct timespec64 t[2];
  bool null;
};

static int utimes_fn(struct inode *i, void *arg) {
  struct times_arg *ta = arg;
  struct cred *c = &cur_proc()->cred;
  if (i->sb->rdonly)
    return -EROFS;
  bool both_now = ta->null || (ta->t[0].nsec == UTIME_NOW && ta->t[1].nsec == UTIME_NOW);
  bool both_omit = !ta->null && ta->t[0].nsec == UTIME_OMIT && ta->t[1].nsec == UTIME_OMIT;
  if (both_omit)
    return 0;
  if (c->fsuid != 0 && c->fsuid != i->uid) {
    if (!both_now)
      return -EPERM;
    if (permission(i, MAY_WRITE))
      return -EACCES;
  }
  uint64_t ns = realtime_ns();
  struct timespec64t now = {(int64_t)(ns / 1000000000UL), (int64_t)(ns % 1000000000UL)};
  for (int k = 0; k < 2; k++) {
    struct timespec64t *dst = k == 0 ? &i->atime : &i->mtime;
    if (ta->null || ta->t[k].nsec == UTIME_NOW)
      *dst = now;
    else if (ta->t[k].nsec != UTIME_OMIT) {
      dst->sec = ta->t[k].sec;
      dst->nsec = ta->t[k].nsec;
    }
  }
  i->ctime = now;
  i->dirty = true;
  return 0;
}

long sys_utimensat(long dirfd, long upath, long uts, long flags) {
  if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  struct times_arg ta;
  ta.null = !uts;
  if (uts) {
    if (copy_from_user(ta.t, (void *)uts, sizeof(ta.t)))
      return -EFAULT;
    for (int k = 0; k < 2; k++)
      if (ta.t[k].nsec != UTIME_NOW && ta.t[k].nsec != UTIME_OMIT &&
          (ta.t[k].nsec < 0 || ta.t[k].nsec >= 1000000000))
        return -EINVAL;
  }
  if (!upath) {
    struct file *f = fd_get((int)dirfd);
    if (!f)
      return -EBADF;
    return utimes_fn(f->ino, &ta);
  }
  int lf = ((flags & AT_SYMLINK_NOFOLLOW) ? 0 : LOOKUP_FOLLOW) | ((flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0);
  return path_op((int)dirfd, (const char *)upath, lf, utimes_fn, &ta);
}

long sys_utimes(long upath, long utv) {
  if (!utv)
    return sys_utimensat(AT_FDCWD, upath, 0, 0);
  int64_t tv[4];
  if (copy_from_user(tv, (void *)utv, sizeof(tv)))
    return -EFAULT;
  struct times_arg ta;
  ta.null = false;
  ta.t[0].sec = tv[0], ta.t[0].nsec = tv[1] * 1000;
  ta.t[1].sec = tv[2], ta.t[1].nsec = tv[3] * 1000;
  return path_op(AT_FDCWD, (const char *)upath, LOOKUP_FOLLOW, utimes_fn, &ta);
}

static int truncate_fn(struct inode *i, void *arg) {
  int64_t len = *(int64_t *)arg;
  if (S_ISDIR(i->mode))
    return -EISDIR;
  if (!S_ISREG(i->mode))
    return -EINVAL;
  int e = permission(i, MAY_WRITE);
  if (e)
    return e;
  if ((uint64_t)len > cur_proc()->rlim[RLIMIT_FSIZE].cur) {
    signal_simple(cur_proc(), SIGXFSZ);
    return -EFBIG;
  }
  if (!i->iops->truncate)
    return -EINVAL;
  e = i->iops->truncate(i, (uint64_t)len);
  if (!e)
    inode_set_times(i, false, true, true);
  return e;
}

long sys_truncate(long path, long len) {
  if (len < 0)
    return -EINVAL;
  int64_t l = len;
  return path_op(AT_FDCWD, (const char *)path, LOOKUP_FOLLOW, truncate_fn, &l);
}

long sys_ftruncate(long fd, long len) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (len < 0)
    return -EINVAL;
  if ((f->flags & O_ACCMODE) == O_RDONLY || !S_ISREG(f->ino->mode))
    return -EINVAL;
  if (!f->ino->iops->truncate)
    return -EINVAL;
  if ((uint64_t)len > cur_proc()->rlim[RLIMIT_FSIZE].cur) {
    signal_simple(cur_proc(), SIGXFSZ);
    return -EFBIG;
  }
  int e = f->ino->iops->truncate(f->ino, (uint64_t)len);
  if (!e)
    inode_set_times(f->ino, false, true, true);
  return e;
}

long sys_fallocate(long fd, long mode, long off, long len) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (mode)
    return -EOPNOTSUPP;
  if (off < 0 || len <= 0)
    return -EINVAL;
  if ((f->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;
  if (!S_ISREG(f->ino->mode))
    return S_ISFIFO(f->ino->mode) ? -ESPIPE : -ENODEV;
  uint64_t end = (uint64_t)off + (uint64_t)len;
  if (end > f->ino->size) {
    if (end > cur_proc()->rlim[RLIMIT_FSIZE].cur)
      return -EFBIG;
    int e = f->ino->iops->truncate(f->ino, end);
    if (e)
      return e;
    inode_set_times(f->ino, false, true, true);
  }
  return 0;
}

long sys_fadvise64(long fd, long off, long len, long advice) {
  (void)off;
  (void)len;
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (f->ino && S_ISFIFO(f->ino->mode))
    return -ESPIPE;
  if (advice < 0 || advice > 5)
    return -EINVAL;
  return 0;
}

struct access_arg {
  int mode;
  bool eff;
};

static int access_fn(struct inode *i, void *arg) {
  struct access_arg *a = arg;
  if (!a->mode)
    return 0;
  struct cred *c = &cur_proc()->cred;
  uid_t su = c->fsuid;
  gid_t sg = c->fsgid;
  if (!a->eff) {
    c->fsuid = c->uid;
    c->fsgid = c->gid;
  }
  int e = permission(i, a->mode);
  c->fsuid = su;
  c->fsgid = sg;
  return e;
}

long sys_faccessat2(long dirfd, long path, long mode, long flags) {
  if (mode & ~7)
    return -EINVAL;
  if (flags & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  struct access_arg a = {(int)mode, (flags & AT_EACCESS) != 0};
  int lf = ((flags & AT_SYMLINK_NOFOLLOW) ? 0 : LOOKUP_FOLLOW) | ((flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0);
  return path_op((int)dirfd, (const char *)path, lf, access_fn, &a);
}
long sys_faccessat(long dirfd, long path, long mode) { return sys_faccessat2(dirfd, path, mode, 0); }
long sys_access(long path, long mode) { return sys_faccessat2(AT_FDCWD, path, mode, 0); }

/* ---- directories ---- */

static int set_dir(struct path *slot, int dirfd, const char *upath) {
  struct path p;
  int e;
  if (!upath) {
    struct file *f = fd_get(dirfd);
    if (!f)
      return -EBADF;
    p.mnt = f->mnt;
    p.ino = igrab(f->ino);
  } else {
    char *path = user_path(upath);
    if (IS_ERR(path))
      return (int)PTR_ERR(path);
    e = namei_at(AT_FDCWD, path, LOOKUP_FOLLOW, &p);
    kfree(path);
    if (e)
      return e;
  }
  if (!S_ISDIR(p.ino->mode)) {
    path_put(&p);
    return -ENOTDIR;
  }
  if ((e = permission(p.ino, MAY_EXEC))) {
    path_put(&p);
    return e;
  }
  path_put(slot);
  *slot = p;
  return 0;
}

long sys_chdir(long path) { return set_dir(&cur_proc()->cwd, 0, (const char *)path); }
long sys_fchdir(long fd) { return set_dir(&cur_proc()->cwd, (int)fd, NULL); }
long sys_chroot(long path) {
  if (!capable())
    return -EPERM;
  return set_dir(&cur_proc()->root, 0, (const char *)path);
}

long sys_getcwd(long ubuf, long size) {
  char *buf = kmalloc(4096);
  if (!buf)
    return -ENOMEM;
  struct path *cwd = &cur_proc()->cwd;
  int e = path_string(cwd, buf, 4096);
  if (e == -ENAMETOOLONG || (e == 0 && cwd->ino->nlink == 0 && S_ISDIR(cwd->ino->mode))) {
    kfree(buf);
    return e ? e : -ENOENT;
  }
  if (e) {
    kfree(buf);
    return e;
  }
  long n = (long)strlen(buf) + 1;
  if (n > size) {
    kfree(buf);
    return -ERANGE;
  }
  if (copy_to_user((void *)ubuf, buf, (size_t)n))
    n = -EFAULT;
  kfree(buf);
  return n;
}

struct dirent_buf {
  char *ubuf;
  size_t size, used;
  int err;
};

static bool fill64(struct dir_ctx *ctx, const char *name, size_t len, ino_t ino, unsigned type, off_t next) {
  struct dirent_buf *b = ctx->priv;
  size_t reclen = (19 + len + 1 + 7) & ~7UL;
  if (b->used + reclen > b->size) {
    if (b->used == 0)
      b->err = -EINVAL;
    return false;
  }
  char rec[19 + 256 + 8];
  memset(rec, 0, reclen);
  uint64_t i = ino;
  int64_t off = next;
  uint16_t rl = (uint16_t)reclen;
  memcpy(rec, &i, 8);
  memcpy(rec + 8, &off, 8);
  memcpy(rec + 16, &rl, 2);
  rec[18] = (char)type;
  memcpy(rec + 19, name, len);
  if (copy_to_user(b->ubuf + b->used, rec, reclen)) {
    b->err = -EFAULT;
    return false;
  }
  b->used += reclen;
  return true;
}

long sys_getdents64(long fd, long ubuf, long size) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (!S_ISDIR(f->ino->mode))
    return -ENOTDIR;
  if (!f->fops || !f->fops->readdir)
    return -ENOTDIR;
  if (!user_range_ok((uint64_t)ubuf, (size_t)size))
    return -EFAULT;
  struct dirent_buf b = {(char *)ubuf, (size_t)size, 0, 0};
  struct dir_ctx ctx = {fill64, &b};
  if (f->ino->nlink == 0)
    return 0;
  int e = f->fops->readdir(f, &ctx);
  if (e < 0)
    return e;
  if (b.err && b.used == 0)
    return b.err;
  inode_set_times(f->ino, true, false, false);
  return (long)b.used;
}

/* ---- file systems ---- */

static long statfs_inode(struct inode *i, void *ubuf) {
  struct statfs64k st;
  memset(&st, 0, sizeof(st));
  st.namelen = 255;
  st.bsize = st.frsize = i->sb->blocksize ? i->sb->blocksize : 4096;
  if (i->sb->ops->statfs) {
    int e = i->sb->ops->statfs(i->sb, &st);
    if (e)
      return e;
  }
  st.type = (int64_t)i->sb->magic;
  st.fsid[0] = (int32_t)i->sb->dev;
  if (i->sb->rdonly)
    st.flags |= 1;
  return copy_to_user(ubuf, &st, sizeof(st)) ? -EFAULT : 0;
}

long sys_statfs(long upath, long ubuf) {
  char *path = user_path((const char *)upath);
  if (IS_ERR(path))
    return PTR_ERR(path);
  struct path p;
  int e = namei_at(AT_FDCWD, path, LOOKUP_FOLLOW, &p);
  kfree(path);
  if (e)
    return e;
  long r = statfs_inode(p.ino, (void *)ubuf);
  path_put(&p);
  return r;
}

long sys_fstatfs(long fd, long ubuf) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  return statfs_inode(f->ino, (void *)ubuf);
}

long sys_sync(void) {
  sync_all();
  return 0;
}

long sys_syncfs(long fd) {
  if (!fd_get((int)fd))
    return -EBADF;
  sync_all();
  return 0;
}

long sys_fsync(long fd) {
  struct file *f = fd_get((int)fd);
  if (!f)
    return -EBADF;
  if (f->fops && f->fops->fsync)
    return f->fops->fsync(f);
  if (f->ino && (S_ISREG(f->ino->mode) || S_ISDIR(f->ino->mode))) {
    sync_all();
    return 0;
  }
  return -EINVAL;
}
long sys_fdatasync(long fd) { return sys_fsync(fd); }

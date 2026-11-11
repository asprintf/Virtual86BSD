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

/* vfs.c - file system types, the inode cache, mounts and path lookup. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>

static struct fs_type *fs_types;
struct list mounts = LIST_INIT(mounts);
struct mount *root_mount;
static struct list icache = LIST_INIT(icache);

void register_fs(struct fs_type *t) {
  t->next = fs_types;
  fs_types = t;
}

struct fs_type *find_fs(const char *name) {
  for (struct fs_type *t = fs_types; t; t = t->next)
    if (!strcmp(t->name, name))
      return t;
  return NULL;
}

struct super *super_alloc(const struct fs_type *t, struct blockdev *b) {
  static unsigned anon_minor = 1;
  struct super *sb = kzalloc(sizeof(*sb));
  if (!sb)
    return NULL;
  sb->type = t;
  sb->bdev = b;
  unsigned mi = anon_minor++;
  sb->dev = b ? b->dev : MKDEV(0, mi);
  return sb;
}

/* ---- inodes ---- */

struct inode *new_inode(struct super *sb, ino_t ino) {
  struct inode *i = kzalloc(sizeof(*i));
  if (!i)
    return NULL;
  i->sb = sb;
  i->ino = ino;
  i->ref = 1;
  list_init(&i->locks);
  waitq_init(&i->lockwait);
  list_add(&icache, &i->cache_link);
  return i;
}

struct inode *iget(struct super *sb, ino_t ino) {
  list_for_each(p, &icache) {
    struct inode *i = list_entry(p, struct inode, cache_link);
    if (i->sb == sb && i->ino == ino) {
      i->ref++;
      return i;
    }
  }
  struct inode *i = new_inode(sb, ino);
  if (!i)
    return ERR_PTR(-ENOMEM);
  int e = sb->ops->read_inode(i);
  if (e) {
    list_del(&i->cache_link);
    kfree(i);
    return ERR_PTR(e);
  }
  return i;
}

struct inode *igrab(struct inode *i) {
  i->ref++;
  return i;
}

void mark_dirty(struct inode *i) { i->dirty = true; }

void inode_set_times(struct inode *i, bool a, bool m, bool c) {
  uint64_t ns = realtime_ns();
  struct timespec64t t = {(int64_t)(ns / 1000000000UL), (int64_t)(ns % 1000000000UL)};
  if (a)
    i->atime = t;
  if (m)
    i->mtime = t;
  if (c)
    i->ctime = t;
  i->dirty = true;
}

void iput(struct inode *i) {
  if (!i)
    return;
  KASSERT(i->ref > 0);
  if (--i->ref > 0)
    return;
  pcache_release(i);
  if (i->nlink == 0 && i->sb->ops->evict) {
    i->sb->ops->evict(i);
  } else if (i->dirty && i->sb->ops->write_inode) {
    i->sb->ops->write_inode(i);
    i->dirty = false;
  }
  if (i->pipe)
    pipe_free_inode(i);
  if (i->sb->ops->put)
    i->sb->ops->put(i);
  list_del(&i->cache_link);
  kfree(i);
}

void sync_all(void) {
  /* page caches first (they set times); writing back sleeps, so the
     inodes are collected, and held, first */
  size_t n = 0, k = 0;
  list_for_each(p, &icache) n += list_entry(p, struct inode, cache_link)->pcache != NULL;
  struct inode **v = n ? kmalloc(n * sizeof(*v)) : NULL;
  if (v) {
    list_for_each(p, &icache) {
      struct inode *i = list_entry(p, struct inode, cache_link);
      if (i->pcache && k < n)
        v[k++] = i, i->ref++;
    }
    for (size_t j = 0; j < k; j++) {
      pcache_sync(v[j]);
      iput(v[j]);
    }
    kfree(v);
  }
  list_for_each(p, &icache) {
    struct inode *i = list_entry(p, struct inode, cache_link);
    if (i->dirty && i->nlink && i->sb->ops->write_inode) {
      i->sb->ops->write_inode(i);
      i->dirty = false;
    }
  }
  list_for_each(p, &mounts) {
    struct mount *m = list_entry(p, struct mount, link);
    if (m->sb->ops->sync)
      m->sb->ops->sync(m->sb);
  }
  bsync(NULL); /* the buffers, letting others run meanwhile */
}

/* ---- permissions ---- */

bool owner_or_root(struct inode *i) {
  struct cred *c = &cur_proc()->cred;
  return c->fsuid == 0 || c->fsuid == i->uid;
}

int permission(struct inode *i, int mask) {
  struct proc *p = current ? current->proc : NULL;
  if (!p)
    return 0;
  struct cred *c = &p->cred;
  if ((mask & MAY_WRITE) && i->sb->rdonly && (S_ISREG(i->mode) || S_ISDIR(i->mode) || S_ISLNK(i->mode)))
    return -EROFS;
  if (c->fsuid == 0) {
    /* root: execute needs some x bit (or a directory) */
    if ((mask & MAY_EXEC) && !S_ISDIR(i->mode) && !(i->mode & 0111))
      return -EACCES;
    return 0;
  }
  unsigned m;
  if (c->fsuid == i->uid)
    m = (i->mode >> 6) & 7;
  else if (in_group(i->gid))
    m = (i->mode >> 3) & 7;
  else
    m = i->mode & 7;
  return (m & (unsigned)mask) == (unsigned)mask ? 0 : -EACCES;
}

/* ---- paths ---- */

void path_get(struct path *p) {
  if (p->ino)
    igrab(p->ino);
}

void path_put(struct path *p) {
  if (p->ino)
    iput(p->ino);
  p->ino = NULL;
}

/* follow mounts down from p */
static void cross_down(struct path *p) {
  while (p->ino->mounted) {
    struct mount *m = p->ino->mounted;
    iput(p->ino);
    p->mnt = m;
    p->ino = igrab(m->root);
  }
}

static struct mount *mount_on(struct mount *parent, struct inode *ino) {
  (void)parent;
  return ino->mounted;
}

/* one step of lookup in directory dir (a path): name of len bytes */
struct inode *lookup_one(struct path *dir, const char *name, size_t len, struct path *out) {
  struct inode *d = dir->ino;
  if (len == 1 && name[0] == '.') {
    out->mnt = dir->mnt;
    out->ino = igrab(d);
    return out->ino;
  }
  if (len == 2 && name[0] == '.' && name[1] == '.') {
    struct path r = cur_proc()->root;
    struct mount *m = dir->mnt;
    struct inode *at = d;
    /* at the root of the process: stays */
    if (at == r.ino && m == r.mnt) {
      out->mnt = m;
      out->ino = igrab(at);
      return out->ino;
    }
    /* at the root of a mount: go to the mount point, then up */
    while (at == m->root && m != m->mountpoint.mnt) {
      at = m->mountpoint.ino;
      m = m->mountpoint.mnt;
      if (at == r.ino && m == r.mnt) {
        out->mnt = m;
        out->ino = igrab(at);
        return out->ino;
      }
    }
    struct inode *i = at->iops->lookup(at, "..", 2);
    if (IS_ERR(i))
      return i;
    out->mnt = m;
    out->ino = i;
    return i;
  }
  if (!d->iops || !d->iops->lookup)
    return ERR_PTR(-ENOTDIR);
  struct inode *i = d->iops->lookup(d, name, len);
  if (IS_ERR(i))
    return i;
  out->mnt = dir->mnt;
  out->ino = i;
  (void)mount_on;
  cross_down(out);
  return out->ino;
}

static int start_path(int dirfd, const char *path, struct path *p) {
  struct proc *pr = cur_proc();
  if (path[0] == '/') {
    *p = pr->root;
  } else if (dirfd == AT_FDCWD) {
    *p = pr->cwd;
  } else {
    struct file *f = fd_get(dirfd);
    if (!f)
      return -EBADF;
    p->mnt = f->mnt;
    p->ino = f->ino;
    if (!S_ISDIR(f->ino->mode))
      return -ENOTDIR;
  }
  path_get(p);
  return 0;
}

#define MAX_LINKS 40

/* walk: resolve path from *cur (consumed); if parent_only, stop before the
   last component and return it in *last and *lastlen */
static int walk(struct path *cur, const char *path, int flags, int *links, bool parent_only,
                const char **last, size_t *lastlen) {
  const char *s = path;
  for (;;) {
    while (*s == '/')
      s++;
    if (!*s) {
      if (parent_only) {
        *last = ".";
        *lastlen = 1;
      }
      return 0;
    }
    const char *e = s;
    while (*e && *e != '/')
      e++;
    size_t len = (size_t)(e - s);
    if (len > 255)
      return -ENAMETOOLONG;
    const char *rest = e;
    while (*rest == '/')
      rest++;
    bool final = !*rest;
    if (!S_ISDIR(cur->ino->mode))
      return -ENOTDIR;
    int pe = permission(cur->ino, MAY_EXEC);
    if (pe)
      return pe;
    if (final && parent_only) {
      *last = s;
      *lastlen = len;
      /* a trailing slash makes the last component a directory */
      return 0;
    }
    struct path next;
    struct inode *i = lookup_one(cur, s, len, &next);
    if (IS_ERR(i))
      return (int)PTR_ERR(i);
    if (S_ISLNK(i->mode) && (!final || (flags & LOOKUP_FOLLOW) || *e == '/')) {
      if (++*links > MAX_LINKS) {
        path_put(&next);
        return -ELOOP;
      }
      if (i->iops->follow) {
        struct path np;
        int r = i->iops->follow(i, &np);
        path_put(&next);
        if (r)
          return r;
        path_put(cur);
        *cur = np;
        if (final) {
          if (*e == '/' && !S_ISDIR(cur->ino->mode))
            return -ENOTDIR;
          return 0;
        }
        s = rest;
        continue;
      }
      char *target = kmalloc(4096);
      if (!target) {
        path_put(&next);
        return -ENOMEM;
      }
      ssize_t n = i->iops->readlink(i, target, 4095);
      path_put(&next);
      if (n < 0) {
        kfree(target);
        return (int)n;
      }
      target[n] = 0;
      if (n == 0) {
        kfree(target);
        return -ENOENT;
      }
      if (target[0] == '/') {
        path_put(cur);
        *cur = cur_proc()->root;
        path_get(cur);
      }
      int r = walk(cur, target, LOOKUP_FOLLOW, links, false, NULL, NULL);
      kfree(target);
      if (r)
        return r;
    } else {
      path_put(cur);
      *cur = next;
    }
    if (final) {
      if (*e == '/' && !S_ISDIR(cur->ino->mode))
        return -ENOTDIR;
      return 0;
    }
    s = rest;
  }
}

int namei_at(int dirfd, const char *path, int flags, struct path *out) {
  if (!*path && !(flags & LOOKUP_EMPTY))
    return -ENOENT;
  struct path cur;
  int e = start_path(dirfd, path, &cur);
  if (e)
    return e;
  int links = 0;
  e = walk(&cur, path, flags, &links, false, NULL, NULL);
  if (e) {
    path_put(&cur);
    return e;
  }
  if ((flags & LOOKUP_DIRECTORY) && !S_ISDIR(cur.ino->mode)) {
    path_put(&cur);
    return -ENOTDIR;
  }
  *out = cur;
  return 0;
}

int nameiparent_at(int dirfd, const char *path, struct path *parent, const char **last, size_t *len) {
  if (!*path)
    return -ENOENT;
  struct path cur;
  int e = start_path(dirfd, path, &cur);
  if (e)
    return e;
  int links = 0;
  e = walk(&cur, path, 0, &links, true, last, len);
  if (e) {
    path_put(&cur);
    return e;
  }
  if (!S_ISDIR(cur.ino->mode)) {
    path_put(&cur);
    return -ENOTDIR;
  }
  *parent = cur;
  return 0;
}

/* the name of child (inode number ino) in directory dir */
struct name_search {
  ino_t ino;
  char *buf;
  bool found;
};

static bool find_name(struct dir_ctx *ctx, const char *name, size_t len, ino_t ino, unsigned type,
                      off_t next) {
  (void)type;
  (void)next;
  struct name_search *ns = ctx->priv;
  if (ino == ns->ino && !(len == 1 && name[0] == '.') && !(len == 2 && name[0] == '.' && name[1] == '.')) {
    memcpy(ns->buf, name, len);
    ns->buf[len] = 0;
    ns->found = true;
    return false;
  }
  return true;
}

static int name_in_dir(struct inode *dir, ino_t ino, char *buf) {
  struct file f;
  memset(&f, 0, sizeof(f));
  f.ino = dir;
  f.fops = dir->fops;
  f.ref = 1;
  struct name_search ns = {ino, buf, false};
  struct dir_ctx ctx = {find_name, &ns};
  for (;;) {
    off_t before = f.pos;
    int e = dir->fops->readdir(&f, &ctx);
    if (e < 0)
      return e;
    if (ns.found)
      return 0;
    if (f.pos == before)
      return -ENOENT;
  }
}

/* the absolute path of p (relative to the process root) */
int path_string(struct path *p, char *buf, size_t n) {
  struct path r = cur_proc()->root;
  struct path cur = *p;
  path_get(&cur);
  char *end = buf + n;
  char *s = end;
  *--s = 0;
  int e = 0;
  char name[256];
  for (int depth = 0; depth < 4096; depth++) {
    if (cur.ino == r.ino && cur.mnt == r.mnt)
      break;
    /* at a mount root: continue from the mount point */
    if (cur.ino == cur.mnt->root) {
      if (cur.mnt->mountpoint.mnt == cur.mnt)
        break; /* the global root */
      struct path mp = cur.mnt->mountpoint;
      path_get(&mp);
      path_put(&cur);
      cur = mp;
      continue;
    }
    struct path parent;
    struct inode *pi = lookup_one(&cur, "..", 2, &parent);
    if (IS_ERR(pi)) {
      e = (int)PTR_ERR(pi);
      break;
    }
    e = name_in_dir(parent.ino, cur.ino->ino, name);
    if (e) {
      path_put(&parent);
      break;
    }
    size_t len = strlen(name);
    if ((size_t)(s - buf) < len + 2) {
      path_put(&parent);
      e = -ENAMETOOLONG;
      break;
    }
    s -= len;
    memcpy(s, name, len);
    *--s = '/';
    path_put(&cur);
    cur = parent;
  }
  path_put(&cur);
  if (e)
    return e;
  if (!*s)
    *--s = '/';
  memmove(buf, s, (size_t)(end - s));
  return 0;
}

/* ---- mounts ---- */

static struct mount *mount_new(struct super *sb, const char *dev, const char *type) {
  struct mount *m = kzalloc(sizeof(*m));
  if (!m)
    return NULL;
  m->sb = sb;
  m->root = igrab(sb->root);
  m->devname = kstrdup(dev ? dev : "none");
  m->fstype = kstrdup(type);
  list_add_tail(&mounts, &m->link);
  return m;
}

int mount_root(const char *type, struct blockdev *b) {
  struct fs_type *t = find_fs(type);
  if (!t)
    return -ENODEV;
  struct super *sb = t->mount(b, 0, NULL);
  if (IS_ERR(sb))
    return (int)PTR_ERR(sb);
  root_mount = mount_new(sb, b ? b->name : type, type);
  if (b) {
    kfree(root_mount->devname);
    char n[32];
    snprintf(n, sizeof(n), "/dev/%s", b->name);
    root_mount->devname = kstrdup(n);
  }
  root_mount->mountpoint.mnt = root_mount;
  root_mount->mountpoint.ino = root_mount->root;
  root_mount->parent = root_mount;
  return 0;
}

static int mount_at(struct path *where, struct super *sb, const char *dev, const char *type,
                    unsigned long flags) {
  if (!S_ISDIR(where->ino->mode))
    return -ENOTDIR;
  if (where->ino->mounted)
    return -EBUSY;
  struct mount *m = mount_new(sb, dev, type);
  if (!m)
    return -ENOMEM;
  m->mountpoint = *where;
  path_get(&m->mountpoint);
  m->parent = where->mnt;
  m->flags = (int)flags;
  where->ino->mounted = m;
  return 0;
}

int do_mount(const char *dev, const char *dir, const char *type, unsigned long flags, const char *data) {
  struct path where;
  int e = namei_at(AT_FDCWD, dir, LOOKUP_FOLLOW, &where);
  if (e)
    return e;
  if (flags & MS_REMOUNT) {
    if (where.ino != where.mnt->root)
      e = -EINVAL;
    else {
      where.mnt->flags = (int)flags;
      where.mnt->sb->rdonly = flags & MS_RDONLY;
      if (where.mnt->sb->ops->sync)
        where.mnt->sb->ops->sync(where.mnt->sb);
      bsync(where.mnt->sb->bdev);
    }
    path_put(&where);
    return e;
  }
  if (flags & MS_BIND) {
    struct path src;
    e = namei_at(AT_FDCWD, dev, LOOKUP_FOLLOW, &src);
    if (!e) {
      struct mount *m = kzalloc(sizeof(*m));
      m->sb = src.mnt->sb;
      m->root = igrab(src.ino);
      m->devname = kstrdup(src.mnt->devname);
      m->fstype = kstrdup(src.mnt->fstype);
      list_add_tail(&mounts, &m->link);
      if (where.ino->mounted)
        e = -EBUSY;
      else {
        m->mountpoint = where;
        path_get(&m->mountpoint);
        m->parent = where.mnt;
        m->flags = (int)flags;
        where.ino->mounted = m;
      }
      path_put(&src);
    }
    path_put(&where);
    return e;
  }
  struct fs_type *t = find_fs(type);
  if (!t) {
    path_put(&where);
    return -ENODEV;
  }
  struct blockdev *b = NULL;
  if (t->needs_dev) {
    struct path dp;
    e = namei_at(AT_FDCWD, dev, LOOKUP_FOLLOW, &dp);
    if (e) {
      path_put(&where);
      return e;
    }
    if (!S_ISBLK(dp.ino->mode))
      e = -ENOTBLK;
    else if (!(b = blockdev_get(dp.ino->rdev)))
      e = -ENXIO;
    path_put(&dp);
    if (e) {
      path_put(&where);
      return e;
    }
    list_for_each(p, &mounts) {
      if (list_entry(p, struct mount, link)->sb->bdev == b) {
        path_put(&where);
        return -EBUSY;
      }
    }
  }
  struct super *sb = t->mount(b, (int)flags, data);
  if (IS_ERR(sb)) {
    path_put(&where);
    return (int)PTR_ERR(sb);
  }
  if (flags & MS_RDONLY)
    sb->rdonly = true;
  e = mount_at(&where, sb, dev, type, flags);
  path_put(&where);
  return e;
}

/* something uses the mount: a process whose working or root directory is in
   it, an open file (a mapped file is held open too) */
static bool mount_in_use(const struct mount *m) {
  list_for_each(p, &all_procs) {
    struct proc *pr = list_entry(p, struct proc, all_link);
    if (pr->cwd.mnt == m || pr->root.mnt == m)
      return true;
    for (int i = 0; i < pr->nfd; i++)
      if (pr->fd[i] && pr->fd[i]->mnt == m)
        return true;
    if (pr->mm) {
      list_for_each(q, &pr->mm->vmas) {
        struct vma *v = list_entry(q, struct vma, link);
        if (v->file && v->file->mnt == m)
          return true;
      }
    }
  }
  return false;
}

static int do_umount(const char *dir) {
  struct path p;
  int e = namei_at(AT_FDCWD, dir, LOOKUP_FOLLOW, &p);
  if (e)
    return e;
  struct mount *m = p.mnt;
  if (p.ino != m->root) {
    path_put(&p); /* a directory that is no mount point */
    return -EINVAL;
  }
  path_put(&p);
  if (m == root_mount)
    return -EBUSY;
  /* busy: something else mounted below, or a process using it */
  list_for_each(q, &mounts) {
    struct mount *o = list_entry(q, struct mount, link);
    if (o != m && o->parent == m)
      return -EBUSY;
  }
  if (mount_in_use(m))
    return -EBUSY;
  bool shared_sb = false;
  list_for_each(q, &mounts) {
    struct mount *o = list_entry(q, struct mount, link);
    if (o != m && o->sb == m->sb)
      shared_sb = true;
  }
  sync_all();
  m->mountpoint.ino->mounted = NULL;
  path_put(&m->mountpoint);
  iput(m->root);
  list_del(&m->link);
  if (!shared_sb && m->sb->ops->umount)
    m->sb->ops->umount(m->sb);
  kfree(m->devname);
  kfree(m->fstype);
  kfree(m);
  return 0;
}

long sys_mount(long udev, long udir, long utype, long flags, long udata) {
  if (!capable())
    return -EPERM;
  char *dev = udev ? user_path((const char *)udev) : kstrdup("none");
  char *dir = user_path((const char *)udir);
  char *type = utype ? user_path((const char *)utype) : kstrdup("");
  long e = 0;
  if (IS_ERR(dev) || IS_ERR(dir) || IS_ERR(type))
    e = -EFAULT;
  else
    e = do_mount(dev, dir, type, (unsigned long)flags, NULL);
  (void)udata;
  if (!IS_ERR(dev))
    kfree(dev);
  if (!IS_ERR(dir))
    kfree(dir);
  if (!IS_ERR(type))
    kfree(type);
  return e;
}

long sys_umount2(long udir, long flags) {
  (void)flags;
  if (!capable())
    return -EPERM;
  char *dir = user_path((const char *)udir);
  if (IS_ERR(dir))
    return PTR_ERR(dir);
  long e = do_umount(dir);
  kfree(dir);
  return e;
}

void vfs_init(void) {
  ext2_register();
  tmpfs_register();
  procfs_register();
}

/* the absolute path of what path names (as /proc/self/fd and exe show it):
   the real path of its directory and its own name; a final symlink is
   followed if follow */
int path_of(int dirfd, const char *path, bool follow, char *out, size_t n) {
  struct path base;
  int e = start_path(dirfd, path, &base);
  if (e)
    return e;
  char *cur = kstrdup(path);
  for (int depth = 0; depth < MAX_LINKS; depth++) {
    const char *last;
    size_t len;
    int links = 0;
    e = walk(&base, cur, 0, &links, true, &last, &len);
    if (e)
      break;
    struct path child;
    struct inode *ci = lookup_one(&base, last, len, &child);
    if (IS_ERR(ci)) {
      e = (int)PTR_ERR(ci);
      break;
    }
    if (S_ISDIR(ci->mode)) {
      e = path_string(&child, out, n);
      path_put(&child);
      break;
    }
    if (S_ISLNK(ci->mode) && follow) {
      char *t = kmalloc(4096);
      ssize_t tl = ci->iops->readlink(ci, t, 4095);
      bool magic = ci->iops->follow != NULL;
      path_put(&child);
      if (tl <= 0) {
        kfree(t);
        e = tl < 0 ? (int)tl : -ENOENT;
        break;
      }
      t[tl] = 0;
      if (magic && t[0] != '/') {
        /* pipe:[N], socket:[N]: the name is the link's text */
        if ((size_t)tl + 1 > n)
          e = -ENAMETOOLONG;
        else
          memcpy(out, t, (size_t)tl + 1);
        kfree(t);
        break;
      }
      if (t[0] == '/') {
        path_put(&base);
        base = cur_proc()->root;
        path_get(&base);
      }
      kfree(cur);
      cur = t;
      continue;
    }
    path_put(&child);
    char name[256];
    memcpy(name, last, len);
    name[len] = 0;
    e = path_string(&base, out, n);
    if (!e) {
      size_t ol = strlen(out);
      if (ol + len + 2 > n)
        e = -ENAMETOOLONG;
      else {
        if (ol > 1)
          out[ol++] = '/';
        memcpy(out + ol, name, len + 1);
      }
    }
    break;
  }
  path_put(&base);
  kfree(cur);
  return e;
}

/* tmpfs.c - a file system in memory.  File contents are shared memory
   objects (so MAP_SHARED maps the file's own pages); directories are lists
   of names.  Also devtmpfs: a tmpfs with the device nodes, on /dev. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "dev.h"

#define TMPFS_MAGIC 0x01021994

struct tdirent {
  struct list link;
  struct inode *ino;
  size_t len;
  char name[];
};

struct tnode {
  struct list entries; /* directories */
  struct inode *parent;
  char *link;          /* symlinks */
  uint64_t dir_seq;    /* directory positions: entry ordinal */
};

static const struct inode_ops tmp_dir_iops, tmp_file_iops, tmp_link_iops, tmp_special_iops;
static const struct file_ops tmp_dir_fops, tmp_file_fops;
static const struct super_ops tmp_sops;
extern const struct file_ops special_fops;

static struct tnode *T(struct inode *i) { return i->priv; }

static struct inode *tmp_new(struct super *sb, mode_t mode, struct inode *dir) {
  struct inode *i = new_inode(sb, ++sb->next_ino);
  if (!i)
    return NULL;
  struct tnode *t = kzalloc(sizeof(*t));
  if (!t) {
    list_del(&i->cache_link);
    kfree(i);
    return NULL;
  }
  list_init(&t->entries);
  i->priv = t;
  i->mode = mode;
  i->nlink = 1;
  if (current && current->proc) {
    struct cred *c = &cur_proc()->cred;
    i->uid = c->fsuid;
    i->gid = c->fsgid;
  }
  if (dir && (dir->mode & S_ISGID)) {
    i->gid = dir->gid;
    if (S_ISDIR(mode))
      i->mode |= S_ISGID;
  }
  inode_set_times(i, true, true, true);
  switch (mode & S_IFMT) {
  case S_IFDIR:
    i->iops = &tmp_dir_iops;
    i->fops = &tmp_dir_fops;
    i->nlink = 2;
    t->parent = dir ? dir : i;
    break;
  case S_IFREG:
    i->iops = &tmp_file_iops;
    i->fops = &tmp_file_fops;
    i->data = shmobj_new(0);
    break;
  case S_IFLNK:
    i->iops = &tmp_link_iops;
    break;
  case S_IFIFO:
    i->iops = &tmp_special_iops;
    i->fops = &fifo_fops;
    break;
  default:
    i->iops = &tmp_special_iops;
    i->fops = &special_fops;
  }
  return i;
}

static struct tdirent *dfind(struct inode *dir, const char *name, size_t len) {
  list_for_each(p, &T(dir)->entries) {
    struct tdirent *d = list_entry(p, struct tdirent, link);
    if (d->len == len && !memcmp(d->name, name, len))
      return d;
  }
  return NULL;
}

static int dadd(struct inode *dir, const char *name, size_t len, struct inode *i) {
  if (len > 255)
    return -ENAMETOOLONG;
  struct tdirent *d = kmalloc(sizeof(*d) + len + 1);
  if (!d)
    return -ENOMEM;
  d->ino = igrab(i); /* the directory holds a reference */
  d->len = len;
  memcpy(d->name, name, len);
  d->name[len] = 0;
  list_add_tail(&T(dir)->entries, &d->link);
  dir->size += 1;
  inode_set_times(dir, false, true, true);
  return 0;
}

static void ddel(struct inode *dir, struct tdirent *d) {
  list_del(&d->link);
  dir->size -= 1;
  inode_set_times(dir, false, true, true);
  iput(d->ino);
  kfree(d);
}

static struct inode *tmp_lookup(struct inode *dir, const char *name, size_t len) {
  if (len == 2 && name[0] == '.' && name[1] == '.')
    return igrab(T(dir)->parent);
  if (len == 1 && name[0] == '.')
    return igrab(dir);
  struct tdirent *d = dfind(dir, name, len);
  return d ? igrab(d->ino) : ERR_PTR(-ENOENT);
}

static struct inode *tmp_create(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t rdev) {
  if (len > 255)
    return ERR_PTR(-ENAMETOOLONG);
  if (dfind(dir, name, len))
    return ERR_PTR(-EEXIST);
  struct inode *i = tmp_new(dir->sb, mode, dir);
  if (!i)
    return ERR_PTR(-ENOSPC);
  i->rdev = rdev;
  int e = dadd(dir, name, len, i);
  if (e) {
    i->nlink = 0;
    iput(i);
    return ERR_PTR(e);
  }
  if (S_ISDIR(mode))
    dir->nlink++;
  return i;
}

static int tmp_link(struct inode *dir, const char *name, size_t len, struct inode *i) {
  int e = dadd(dir, name, len, i);
  if (!e)
    i->nlink++;
  return e;
}

static int tmp_unlink(struct inode *dir, const char *name, size_t len, struct inode *i) {
  struct tdirent *d = dfind(dir, name, len);
  if (!d)
    return -ENOENT;
  i->nlink--;
  inode_set_times(i, false, false, true);
  ddel(dir, d);
  return 0;
}

static int tmp_rmdir(struct inode *dir, const char *name, size_t len, struct inode *i) {
  if (!list_empty(&T(i)->entries))
    return -ENOTEMPTY;
  struct tdirent *d = dfind(dir, name, len);
  if (!d)
    return -ENOENT;
  i->nlink = 0;
  dir->nlink--;
  ddel(dir, d);
  return 0;
}

static int tmp_rename(struct inode *odir, const char *oname, size_t olen, struct inode *i,
                      struct inode *ndir, const char *nname, size_t nlen, struct inode *target) {
  if (nlen > 255)
    return -ENAMETOOLONG;
  bool isdir = S_ISDIR(i->mode);
  if (target) {
    if (isdir && !list_empty(&T(target)->entries))
      return -ENOTEMPTY;
    struct tdirent *t = dfind(ndir, nname, nlen);
    if (isdir) {
      target->nlink = 0;
      ndir->nlink--;
    } else
      target->nlink--;
    ddel(ndir, t);
  }
  int e = dadd(ndir, nname, nlen, i);
  if (e)
    return e;
  struct tdirent *o = dfind(odir, oname, olen);
  if (o)
    ddel(odir, o);
  if (isdir && odir != ndir) {
    T(i)->parent = ndir;
    odir->nlink--;
    ndir->nlink++;
  }
  inode_set_times(i, false, false, true);
  return 0;
}

static struct inode *tmp_symlink(struct inode *dir, const char *name, size_t len, const char *target) {
  struct inode *i = tmp_create(dir, name, len, S_IFLNK | 0777, 0);
  if (IS_ERR(i))
    return i;
  T(i)->link = kstrdup(target);
  i->size = strlen(target);
  return i;
}

static ssize_t tmp_readlink(struct inode *i, char *buf, size_t n) {
  size_t l = MIN(strlen(T(i)->link), n);
  memcpy(buf, T(i)->link, l);
  return (ssize_t)l;
}

static int tmp_truncate(struct inode *i, uint64_t size) {
  if (size > (1UL << 40))
    return -EFBIG;
  int e = shmobj_resize(i->data, (size_t)PAGE_UP(size) >> 12);
  if (e)
    return e;
  /* zero the tail of the last page */
  if (size % PAGE_SIZE && i->data->page[size >> 12])
    memset((char *)P2V(i->data->page[size >> 12]) + size % PAGE_SIZE, 0, PAGE_SIZE - size % PAGE_SIZE);
  i->size = size;
  i->blocks = 0;
  for (size_t k = 0; k < i->data->npages; k++)
    if (i->data->page[k])
      i->blocks += 8;
  return 0;
}

static ssize_t tmp_read(struct file *f, char *buf, size_t n, off_t *pos) {
  struct inode *i = f->ino;
  if ((uint64_t)*pos >= i->size)
    return 0;
  n = MIN(n, (size_t)(i->size - (uint64_t)*pos));
  size_t done = 0;
  while (done < n) {
    uint64_t p = (uint64_t)*pos;
    size_t off = p % PAGE_SIZE, chunk = MIN(n - done, PAGE_SIZE - off);
    uint64_t pg = (p >> 12) < i->data->npages ? i->data->page[p >> 12] : 0;
    int r = pg ? copy_to_user(buf + done, (char *)P2V(pg) + off, chunk) : clear_user(buf + done, chunk);
    if (r)
      return done ? (ssize_t)done : -EFAULT;
    done += chunk;
    *pos += (off_t)chunk;
  }
  inode_set_times(i, true, false, false);
  return (ssize_t)done;
}

static ssize_t tmp_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  struct inode *i = f->ino;
  if (f->flags & O_APPEND)
    *pos = (off_t)i->size;
  uint64_t lim = cur_proc()->rlim[RLIMIT_FSIZE].cur;
  if ((uint64_t)*pos >= lim && n) {
    signal_simple(cur_proc(), SIGXFSZ);
    return -EFBIG;
  }
  if ((uint64_t)*pos + n > lim)
    n = (size_t)(lim - (uint64_t)*pos);
  uint64_t end = (uint64_t)*pos + n;
  if (PAGE_UP(end) >> 12 > i->data->npages && shmobj_resize(i->data, PAGE_UP(end) >> 12))
    return -ENOSPC;
  size_t done = 0;
  while (done < n) {
    uint64_t p = (uint64_t)*pos;
    size_t off = p % PAGE_SIZE, chunk = MIN(n - done, PAGE_SIZE - off);
    uint64_t pg = shmobj_page(i->data, p >> 12);
    if (!pg)
      return done ? (ssize_t)done : -ENOSPC;
    if (copy_from_user((char *)P2V(pg) + off, buf + done, chunk))
      return done ? (ssize_t)done : -EFAULT;
    done += chunk;
    *pos += (off_t)chunk;
    if ((uint64_t)*pos > i->size)
      i->size = (uint64_t)*pos;
  }
  i->blocks = 0;
  for (size_t k = 0; k < i->data->npages; k++)
    if (i->data->page[k])
      i->blocks += 8;
  inode_set_times(i, false, true, true);
  return (ssize_t)done;
}

static int tmp_readdir(struct file *f, struct dir_ctx *ctx) {
  struct inode *dir = f->ino;
  /* positions: 0 ".", 1 "..", then 2 + ordinal in the list */
  if (f->pos == 0) {
    if (!ctx->fill(ctx, ".", 1, dir->ino, DT_DIR, 1))
      return 0;
    f->pos = 1;
  }
  if (f->pos == 1) {
    if (!ctx->fill(ctx, "..", 2, T(dir)->parent->ino, DT_DIR, 2))
      return 0;
    f->pos = 2;
  }
  off_t k = 2;
  list_for_each(p, &T(dir)->entries) {
    if (k++ < f->pos)
      continue;
    struct tdirent *d = list_entry(p, struct tdirent, link);
    if (!ctx->fill(ctx, d->name, d->len, d->ino->ino, IFTODT(d->ino->mode), k))
      return 0;
    f->pos = k;
  }
  return 0;
}

static void tmp_put(struct inode *i) {
  struct tnode *t = T(i);
  if (t) {
    list_for_each_safe(p, n, &t->entries) {
      struct tdirent *d = list_entry(p, struct tdirent, link);
      list_del(&d->link);
      iput(d->ino);
      kfree(d);
    }
    kfree(t->link);
    kfree(t);
  }
  if (i->data)
    shmobj_put(i->data);
}

static int tmp_statfs(struct super *sb, struct statfs64k *st) {
  (void)sb;
  st->bsize = st->frsize = PAGE_SIZE;
  st->blocks = total_pages / 2;
  st->bfree = st->bavail = free_pages_count / 2;
  st->files = 1 << 20;
  st->ffree = 1 << 19;
  return 0;
}

static int tmp_noop_write(struct inode *i) {
  (void)i;
  return 0;
}

static const struct inode_ops tmp_dir_iops = {
    .lookup = tmp_lookup,
    .create = tmp_create,
    .link = tmp_link,
    .unlink = tmp_unlink,
    .rmdir = tmp_rmdir,
    .rename = tmp_rename,
    .symlink = tmp_symlink,
};
static const struct inode_ops tmp_file_iops = {.truncate = tmp_truncate};
static const struct inode_ops tmp_link_iops = {.readlink = tmp_readlink};
static const struct inode_ops tmp_special_iops = {.truncate = NULL};
static const struct file_ops tmp_dir_fops = {.readdir = tmp_readdir, .seekable = true};
static const struct file_ops tmp_file_fops = {.read = tmp_read, .write = tmp_write, .seekable = true};
static const struct super_ops tmp_sops = {
    .read_inode = NULL,
    .write_inode = tmp_noop_write,
    .put = tmp_put,
    .statfs = tmp_statfs,
};

static struct fs_type tmpfs_type, devtmpfs_type;

static struct super *tmp_mount_type(struct fs_type *t) {
  struct super *sb = super_alloc(t, NULL);
  if (!sb)
    return ERR_PTR(-ENOMEM);
  sb->ops = &tmp_sops;
  sb->blocksize = PAGE_SIZE;
  sb->magic = TMPFS_MAGIC;
  struct inode *root = tmp_new(sb, S_IFDIR | 01777, NULL);
  if (!root)
    return ERR_PTR(-ENOMEM);
  root->uid = root->gid = 0;
  sb->root = root;
  return sb;
}

static struct super *tmp_mount(struct blockdev *b, int flags, const char *data) {
  (void)b;
  (void)flags;
  (void)data;
  return tmp_mount_type(&tmpfs_type);
}

struct super *tmpfs_mount_internal(void) { return tmp_mount(NULL, 0, NULL); }

/* devtmpfs: one instance, mounted wherever asked */
static struct super *devtmpfs_sb;

static struct super *dev_mount(struct blockdev *b, int flags, const char *data) {
  (void)b;
  (void)flags;
  (void)data;
  if (!devtmpfs_sb) {
    devtmpfs_sb = tmp_mount_type(&devtmpfs_type);
    if (IS_ERR(devtmpfs_sb)) {
      struct super *s = devtmpfs_sb;
      devtmpfs_sb = NULL;
      return s;
    }
    devtmpfs_sb->root->mode = S_IFDIR | 0755;
    devtmpfs_populate(devtmpfs_sb);
  }
  return devtmpfs_sb;
}

struct inode *tmpfs_mknod_path(struct super *sb, const char *name, mode_t mode, dev_t rdev) {
  struct inode *dir = sb->root;
  const char *slash;
  while ((slash = strchr(name, '/'))) {
    struct inode *d = tmp_lookup(dir, name, (size_t)(slash - name));
    if (IS_ERR(d))
      d = tmp_create(dir, name, (size_t)(slash - name), S_IFDIR | 0755, 0);
    if (IS_ERR(d))
      return d;
    iput(d); /* still held by its directory */
    dir = d;
    name = slash + 1;
  }
  struct inode *i = tmp_create(dir, name, strlen(name), mode, rdev);
  if (!IS_ERR(i)) {
    i->uid = i->gid = 0;
    iput(i);
  }
  return i;
}

static struct fs_type tmpfs_type = {.name = "tmpfs", .mount = tmp_mount};
static struct fs_type devtmpfs_type = {.name = "devtmpfs", .mount = dev_mount};

void tmpfs_register(void) {
  register_fs(&tmpfs_type);
  register_fs(&devtmpfs_type);
  static struct fs_type ramfs = {.name = "ramfs", .mount = tmp_mount};
  register_fs(&ramfs);
}

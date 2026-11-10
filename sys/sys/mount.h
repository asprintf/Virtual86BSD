/* mount.h - file systems and mounts. */
#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/conf.h>

struct statfs64k {
  int64_t type, bsize;
  uint64_t blocks, bfree, bavail, files, ffree;
  int32_t fsid[2];
  int64_t namelen, frsize, flags, spare[4];
};

struct super_ops {
  int (*read_inode)(struct inode *ino);
  int (*write_inode)(struct inode *ino);
  void (*evict)(struct inode *ino); /* last reference to an unlinked inode */
  void (*put)(struct inode *ino);   /* last reference */
  int (*statfs)(struct super *sb, struct statfs64k *st);
  void (*sync)(struct super *sb);
  void (*umount)(struct super *sb);
  void (*shutdown)(struct super *sb); /* the system stops: write all, mark clean */
};

struct super {
  const struct fs_type *type;
  struct blockdev *bdev;
  dev_t dev;
  struct inode *root;
  const struct super_ops *ops;
  void *priv;
  unsigned blocksize;
  bool rdonly;
  uint64_t magic;
  ino_t next_ino;
  bool no_reopen; /* sockets, message queues: not opened by name (/proc) */
};

struct fs_type {
  const char *name;
  bool needs_dev;
  struct super *(*mount)(struct blockdev *b, int flags, const char *data);
  struct fs_type *next;
};

struct mount {
  struct super *sb;
  struct inode *root;
  struct path mountpoint; /* where it is mounted (itself for /) */
  struct mount *parent;
  char *devname, *fstype;
  int flags;
  struct list link;
};
#define MS_RDONLY 1
#define MS_NOSUID 2
#define MS_NODEV 4
#define MS_NOEXEC 8
#define MS_REMOUNT 32
#define MS_BIND 4096


extern struct list mounts;
extern struct mount *root_mount;
void register_fs(struct fs_type *t);
struct fs_type *find_fs(const char *name);
struct super *super_alloc(const struct fs_type *t, struct blockdev *b);
int do_mount(const char *dev, const char *dir, const char *type, unsigned long flags, const char *data);
int mount_root(const char *type, struct blockdev *b);

/* file systems */
void ext2_register(void);
void tmpfs_register(void);
void procfs_register(void);
struct super *tmpfs_mount_internal(void);
int devtmpfs_populate(struct super *sb);
struct inode *devtmpfs_add(const char *name, mode_t mode, dev_t rdev);
void devtmpfs_remove(const char *name);
void ofd_locks_release(struct file *f);
struct inode *tmpfs_mknod_path(struct super *sb, const char *name, mode_t mode, dev_t rdev);

#endif

/* fs.h - the virtual file system. */
#ifndef FS_H
#define FS_H

#include "kernel.h"
#include "proc.h"

struct super;
struct inode;
struct file;
struct mount;
struct blockdev;
struct shmobj;
struct pipe;
struct sock;

/* file types and modes */
#define S_IFMT 0170000
#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* d_type */
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define IFTODT(m) (((m) & S_IFMT) >> 12)

/* open flags (Linux x86-64) */
#define O_ACCMODE 3
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DSYNC 010000
#define O_ASYNC 020000
#define O_DIRECT 040000
#define O_LARGEFILE 0100000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW 0400000
#define O_NOATIME 01000000
#define O_CLOEXEC 02000000
#define O_SYNC 04010000
#define O_PATH 010000000
#define O_TMPFILE 020000000
#define O_CLOFORK 010000000000 /* POSIX.1-2024; Linux lacks it (the value of Shall's C library) */
#define FD_CLOEXEC 1
#define FD_CLOFORK 2

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_EACCESS 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

/* poll */
#define POLLIN 0x001
#define POLLPRI 0x002
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
#define POLLNVAL 0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200

struct timespec64t {
  int64_t sec, nsec;
};

struct vma;
struct inode_ops {
  struct inode *(*lookup)(struct inode *dir, const char *name, size_t len);
  struct inode *(*create)(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t rdev);
  int (*link)(struct inode *dir, const char *name, size_t len, struct inode *ino);
  int (*unlink)(struct inode *dir, const char *name, size_t len, struct inode *ino);
  int (*rmdir)(struct inode *dir, const char *name, size_t len, struct inode *ino);
  /* rename: the target (if any) has been checked; replace it */
  int (*rename)(struct inode *odir, const char *oname, size_t olen, struct inode *ino,
                struct inode *ndir, const char *nname, size_t nlen, struct inode *target);
  struct inode *(*symlink)(struct inode *dir, const char *name, size_t len, const char *target);
  ssize_t (*readlink)(struct inode *ino, char *buf, size_t n); /* kernel buffer */
  int (*truncate)(struct inode *ino, uint64_t size);
  /* "magic" links (procfs fd/N, cwd, root, exe): where following leads */
  int (*follow)(struct inode *ino, struct path *out);
  /* the page cache's access to a regular file's data (kernel buffer,
     within the file's size; writing allocates blocks but never extends):
     a file system that has it can be mapped MAP_SHARED */
  int (*rawio)(struct inode *ino, void *buf, size_t n, uint64_t pos, bool write);
};

struct dir_ctx;
typedef bool (*filldir_t)(struct dir_ctx *ctx, const char *name, size_t len, ino_t ino,
                          unsigned type, off_t next);
struct dir_ctx {
  filldir_t fill;
  void *priv;
};

struct file_ops {
  int (*open)(struct inode *ino, struct file *f);
  void (*release)(struct file *f);
  ssize_t (*read)(struct file *f, char *buf, size_t n, off_t *pos);
  ssize_t (*write)(struct file *f, const char *buf, size_t n, off_t *pos);
  int (*readdir)(struct file *f, struct dir_ctx *ctx);
  long (*ioctl)(struct file *f, unsigned cmd, uint64_t arg);
  unsigned (*poll)(struct file *f);
  int (*fsync)(struct file *f);
  /* a device's own memory for mmap: set v->shm (the VMA exists) */
  int (*mmap)(struct file *f, struct vma *v, uint64_t off, uint64_t len);
  bool seekable;
};

struct inode {
  struct super *sb;
  ino_t ino;
  mode_t mode;
  uint32_t nlink;
  uid_t uid;
  gid_t gid;
  uint64_t size;
  uint64_t blocks; /* 512-byte units */
  dev_t rdev;
  struct timespec64t atime, mtime, ctime;
  int ref;
  bool dirty;
  struct list cache_link;
  const struct inode_ops *iops;
  const struct file_ops *fops;
  void *priv;            /* the file system's */
  struct mount *mounted; /* something is mounted here */
  struct pipe *pipe;     /* FIFO */
  struct sock *sock;     /* bound AF_UNIX socket */
  struct shmobj *data;   /* tmpfs file contents */
  struct shmobj *pcache; /* page cache (shared mappings; fs/pcache.c) */
  struct list locks;     /* POSIX record locks */
  struct waitq lockwait;
};

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

struct file {
  int ref;
  struct inode *ino;
  struct mount *mnt;
  unsigned flags;
  off_t pos;
  const struct file_ops *fops;
  void *priv;
  char *path;
};

/* fs/vfs.c */
extern struct list mounts;
extern struct mount *root_mount;
void vfs_init(void);
void register_fs(struct fs_type *t);
struct fs_type *find_fs(const char *name);
struct inode *iget(struct super *sb, ino_t ino); /* read through the cache */
struct inode *new_inode(struct super *sb, ino_t ino); /* in the cache, not read */
struct inode *igrab(struct inode *ino);
void iput(struct inode *ino);
void mark_dirty(struct inode *ino);
void inode_set_times(struct inode *ino, bool a, bool m, bool c);
void sync_all(void);
struct super *super_alloc(const struct fs_type *t, struct blockdev *b);
int do_mount(const char *dev, const char *dir, const char *type, unsigned long flags, const char *data);
int mount_root(const char *type, struct blockdev *b);
void path_get(struct path *p);
void path_put(struct path *p);
int permission(struct inode *ino, int mask);
bool owner_or_root(struct inode *ino);

/* lookups: flags */
#define LOOKUP_FOLLOW 1
#define LOOKUP_DIRECTORY 2
#define LOOKUP_EMPTY 4
int namei_at(int dirfd, const char *path, int flags, struct path *out);
/* parent directory and last component (pointing into path) */
int nameiparent_at(int dirfd, const char *path, struct path *parent, const char **last, size_t *len);
int path_string(struct path *p, char *buf, size_t n); /* absolute path of directory p */
int path_of(int dirfd, const char *path, bool follow, char *out, size_t n);
struct inode *lookup_one(struct path *dir, const char *name, size_t len, struct path *out);

/* fs/file.c */
struct file *file_alloc(struct inode *ino, struct mount *mnt, unsigned flags);
void file_get(struct file *f);
void file_put(struct file *f);
struct file *fd_get(int fd);  /* no extra reference */
int fd_install(struct file *f, int min, unsigned fdflags); /* FD_CLOEXEC | FD_CLOFORK */
/* descriptor flags from O_CLOEXEC/O_CLOFORK (= SOCK_CLOEXEC/SOCK_CLOFORK) */
static inline unsigned fd_flags_of(long oflags) {
  return ((oflags & O_CLOEXEC) ? FD_CLOEXEC : 0) | ((oflags & O_CLOFORK) ? FD_CLOFORK : 0);
}
int fd_close(int fd);
void fd_close_on_exec(struct proc *p);
void fd_close_all(struct proc *p);
int fds_dup(struct proc *dst, struct proc *src);
/* page cache (fs/pcache.c) */
struct shmobj *pcache_get(struct inode *i);      /* for a mapping: a new reference */
uint64_t pcache_fill(struct shmobj *o, size_t idx);
void *pcache_lookup(struct inode *i, uint64_t pos); /* the cached page holding pos */
void pcache_resize(struct inode *i);             /* after the size changed */
int pcache_sync(struct inode *i);
void pcache_unmapped(struct shmobj *o);         /* a mapping went away */
void pcache_release(struct inode *i);           /* the inode goes away */
void pcache_sync_all(void);
ssize_t kernel_read(struct file *f, void *buf, size_t n, off_t off);
ssize_t kernel_write(struct file *f, const void *buf, size_t n, off_t off);
struct file *open_path(int dirfd, const char *path, int flags, mode_t mode, int *err);
struct file *open_inode(struct inode *ino, struct mount *mnt, int flags, int *err);
int generic_readdir_check(struct file *f);
int fill_stat(struct inode *ino, void *ustat);
long do_ioctl(struct file *f, unsigned cmd, uint64_t arg);

/* fs/pipe.c */
int fifo_open(struct inode *ino, struct file *f);
extern const struct file_ops fifo_fops;
void pipe_free_inode(struct inode *ino);

/* shared poll wait queue: every state change that poll may care about */
extern struct waitq poll_wait;
void poll_wake(void);

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

/* record locks (fs/lock.c) */
long do_fcntl_lock(struct file *f, int cmd, void *uflock);
void locks_release(struct proc *p, struct inode *ino);

/* SysV shm (kern/ipc.c), needed by mm */
void shm_attach_vma(int id);
void shm_detach_vma(int id);

static inline bool is_dir(struct inode *i) { return S_ISDIR(i->mode); }

#endif

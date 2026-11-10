/* vnode.h - inodes (the VFS node): the file constants, inode_ops, struct
   inode, lookups.  mount.h has the file systems, file.h the open files. */
#ifndef _SYS_VNODE_H_
#define _SYS_VNODE_H_

#include <sys/systm.h>
#include <sys/proc.h>

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
#define O_CLOFORK 010000000000 /* POSIX.1-2024; Linux lacks it (the value of V86BSD's C library) */
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
struct file_ops;
struct fs_type;
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

/* fs/vfs.c */
void vfs_init(void);
struct inode *iget(struct super *sb, ino_t ino); /* read through the cache */
struct inode *new_inode(struct super *sb, ino_t ino); /* in the cache, not read */
struct inode *igrab(struct inode *ino);
void iput(struct inode *ino);
void mark_dirty(struct inode *ino);
void inode_set_times(struct inode *ino, bool a, bool m, bool c);
void sync_all(void);
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

/* fs/pipe.c */
int fifo_open(struct inode *ino, struct file *f);
extern const struct file_ops fifo_fops;
void pipe_free_inode(struct inode *ino);

/* shared poll wait queue: every state change that poll may care about */
extern struct waitq poll_wait;
void poll_wake(void);

/* record locks (fs/lock.c) */
long do_fcntl_lock(struct file *f, int cmd, void *uflock);
void locks_release(struct proc *p, struct inode *ino);

/* SysV shm (kern/ipc.c), needed by mm */
void shm_attach_vma(int id);
void shm_detach_vma(int id);

static inline bool is_dir(struct inode *i) { return S_ISDIR(i->mode); }

#endif

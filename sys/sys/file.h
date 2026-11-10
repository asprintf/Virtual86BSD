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

/* file.h - open files: struct file, file_ops, descriptors, the page cache of
   files (kern/vfs_pcache.c). */
#ifndef _SYS_FILE_H_
#define _SYS_FILE_H_

#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>

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

struct file {
  int ref;
  struct inode *ino;
  struct mount *mnt;
  unsigned flags;
  off_t pos;
  const struct file_ops *fops;
  const struct file_ops *rfops; /* revoked (revoke(2)): fops before it, else NULL */
  void *priv;
  char *path;
};

/* the device behind this open file was revoked: it is a dead file now */
static inline bool file_revoked(const struct file *f) { return f->rfops != NULL; }

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
/* miscfs/deadfs: revoke(2), every open file of the device of ino */
int revoke_dev(struct inode *ino);
/* tty.c: terminal files, for revoke_dev() */
bool tty_file_p(const struct file *f);
void tty_file_revoked(struct file *f);

#endif

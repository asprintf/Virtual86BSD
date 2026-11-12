/*
 * Copyright (c) 2026, asprintf
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

/*
 * ffs_vfs.h - the FFS2 driver's private interface (V86BSD).
 *
 * The driver is OpenBSD's FFS (sys/ufs) ported onto V86BSD's file system
 * layer (struct inode_ops and friends, the buffer cache of kern/vfs_bio.c).
 * The buffer cache keys its buffers by (device, block number, size) and
 * keeps no coherence between sizes, so every buffer the driver uses has
 * ONE size, the fragment size: bread(dev, fragment number, fs_fsize).
 * Whatever is larger than a fragment (a block, a cylinder group block, the
 * super-block) is read and written as several fragment buffers, by
 * ffs_rw().
 */

#ifndef _UFS_FFS_FFS_VFS_H_
#define _UFS_FFS_FFS_VFS_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/dinode.h>
#undef IFTODT			/* (sys/vnode.h has it too, the same) */
#include <ufs/ufs/dir.h>

typedef unsigned int u_int;
typedef unsigned char u_char;
typedef uint8_t u_int8_t;

/* one mounted file system (the super block's priv) */
struct ffsmount {
	struct super	*um_sb;
	struct blockdev	*um_bdev;
	struct fs	*um_fs;		/* the super-block, in memory */
	bool		 um_rw;		/* mounted (marked in use) for writing */
	bool		 um_unclean;	/* it was not clean when mounted */
	bool		 um_damaged;	/* found damaged: read-only, stays marked in use */
	uint64_t	 um_maxfilesize; /* the largest file: the super-block's, within the kernel's */
};

/* an inode in memory: the on-disk inode, kept whole (its fields that the
 * VFS inode has too are the VFS inode's, and written back from it) */
struct ffs_ino {
	struct ufs2_dinode	i_din;
};

#define VFSTOFFS(sb)	((struct ffsmount *)(sb)->priv)
#define ITOUM(ip)	VFSTOFFS((ip)->sb)
#define ITOFS(ip)	(ITOUM(ip)->um_fs)
#define DINO(ip)	(&((struct ffs_ino *)(ip)->priv)->i_din)

/* size of block lbn of a file of size `size' (OpenBSD's blksize()) */
#define ffs_blksize(fs, size, lbn) sblksize(fs, size, lbn)

/* ffs_vfsops.c */
void	ffs_register(void);
void	ffs_rw(struct ffsmount *, daddr_t, uint64_t, void *, size_t, bool, bool);
struct cg *ffs_cgread(struct ffsmount *, u_int);
void	ffs_cgwrite(struct ffsmount *, u_int, struct cg *);
int	ffs_update(struct inode *, bool);
void	ffs_sbupdate(struct ffsmount *, bool);
uint32_t ffs_random(uint32_t);
void	ffs_fserr(struct fs *, const char *);
void	ffs_damaged(struct ffsmount *);

/* ffs_alloc.c */
int	ffs_alloc(struct inode *, daddr_t, daddr_t, int, daddr_t *);
int	ffs_realloccg(struct inode *, daddr_t, daddr_t, int, int, daddr_t *);
int	ffs_inode_alloc(struct inode *, mode_t, ino_t *);
void	ffs_blkfree(struct inode *, daddr_t, long);
void	ffs_freefile(struct inode *, ino_t, mode_t);
int64_t	ffs2_blkpref(struct inode *, daddr_t, int, int64_t);

/* ffs_balloc.c */
int	ffs_bmap(struct inode *, daddr_t, daddr_t *);
int	ffs_balloc(struct inode *, uint64_t, int, daddr_t *);
int	ffs_truncate(struct inode *, uint64_t);

/* ffs_vnops.c */
extern const struct inode_ops ffs_dir_iops, ffs_file_iops, ffs_link_iops, ffs_special_iops;
extern const struct file_ops ffs_dir_fops, ffs_file_fops;
void	ffs_set_ops(struct inode *);

#endif /* !_UFS_FFS_FFS_VFS_H_ */

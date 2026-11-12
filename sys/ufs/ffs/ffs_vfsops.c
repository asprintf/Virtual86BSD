/*	$OpenBSD: ffs_vfsops.c,v 1.201 2025/09/20 13:53:36 mpi Exp $	*/
/*	$NetBSD: ffs_vfsops.c,v 1.19 1996/02/09 22:22:26 christos Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 */

/*
 * Adapted for V86BSD from OpenBSD sys/ufs/ffs/ffs_vfsops.c (revision 1.201).
 * Changes: ffs_mountfs(), ffs_validate(), ffs_sbupdate(), ffs_statfs(), ffs_unmount() and the
 *   inode read, write and inactive functions on V86BSD's super_ops: FFS2 only, no
 *   quota, no snapshots, no soft updates; a file system that is not clean, or has
 *   cluster maps, is mounted read-only; the mount is marked in use and clean again when
 *   it is remounted read-write and read-only; every size the super-block gives is
 *   checked; otherwise as OpenBSD.
 */

#include <ufs/ffs/ffs_vfs.h>

extern const struct file_ops special_fops;

static const struct super_ops ffs_sops;

/*
 * A random number below n (the generation numbers of inodes, the
 * cylinder group a new top level directory goes to).
 */
uint32_t
ffs_random(uint32_t n)
{
	uint32_t r;

	if (n <= 1)
		return 0;
	get_random_bytes(&r, sizeof(r));
	return r % n;
}

/* a message about the file system (OpenBSD logs "uid %u on %s: %s") */
void
ffs_fserr(struct fs *fs, const char *cp)
{
	static uint64_t last;
	uint64_t now = realtime_ns();

	if (now - last < 2000000000UL)	/* (ratecheck of two seconds) */
		return;
	last = now;
	printf("ffs: uid %u: %s\n", (unsigned)cur_proc()->cred.fsuid, cp);
	(void)fs;
}

/*
 * The driver found the file system damaged (a map or a block number that
 * is wrong; OpenBSD panics): no more writes, and it is not marked clean
 * again, so that the next mount asks for fsck_ffs.
 */
void
ffs_damaged(struct ffsmount *um)
{
	if (um->um_damaged)
		return;
	um->um_damaged = true;
	um->um_sb->rdonly = true;
	printf("ffs: %s: damaged, now read-only: unmount it and run fsck_ffs\n",
	    um->um_bdev->name);
}

/*
 * Read or write n bytes at the byte offset off from fragment fsb, through
 * the buffer cache, in buffers of one fragment each.  A write of whole
 * fragments does not read what it replaces.  sync: the buffers go to the
 * device before this returns (metadata that others depend on), else they
 * are written later.  A write of p == NULL writes zeroes.
 */
void
ffs_rw(struct ffsmount *um, daddr_t fsb, uint64_t off, void *p, size_t n,
    bool write, bool sync)
{
	unsigned fsize = um->um_fs->fs_fsize;
	uint64_t pos = (uint64_t)fsb * fsize + off;
	char *cp = p;

	while (n > 0) {
		uint64_t b = pos / fsize;
		unsigned o = (unsigned)(pos % fsize);
		size_t c = MIN(n, (size_t)(fsize - o));
		struct buf *bp;

		if (write && o == 0 && c == fsize)
			bp = bget(um->um_bdev, b, fsize);
		else
			bp = bread(um->um_bdev, b, fsize);
		if (write) {
			if (cp != NULL)
				memcpy(bp->data + o, cp, c);
			else
				memset(bp->data + o, 0, c);
			if (sync)
				bwritenow(bp);
			else
				bdirty(bp);
		} else
			memcpy(cp, bp->data + o, c);
		brelse(bp);
		if (cp != NULL)
			cp += c;
		pos += c;
		n -= c;
	}
}

/*
 * The cylinder group block of cg, copied out of the buffer cache; NULL if
 * it is not a cylinder group (a damaged file system).  ffs_cgwrite() puts
 * it back (into the buffers, written later) and frees the copy; whoever
 * does not write it kfree()s it.
 */
struct cg *
ffs_cgread(struct ffsmount *um, u_int c)
{
	struct fs *fs = um->um_fs;
	struct cg *cgp = kmalloc(fs->fs_cgsize);

	if (cgp == NULL)
		return NULL;
	ffs_rw(um, cgtod(fs, c), 0, cgp, fs->fs_cgsize, false, false);
	if (!cg_chkmagic(cgp) || cgp->cg_cgx != c) {
		kfree(cgp);
		return NULL;
	}
	return cgp;
}

void
ffs_cgwrite(struct ffsmount *um, u_int c, struct cg *cgp)
{
	struct fs *fs = um->um_fs;

	ffs_rw(um, cgtod(fs, c), 0, cgp, fs->fs_cgsize, true, false);
	kfree(cgp);
}

/*
 * Write the summary information and the super-block back.  The in-memory
 * pointers of struct fs are not written.
 */
void
ffs_sbupdate(struct ffsmount *um, bool sync)
{
	struct fs *fs = um->um_fs, *dfs;

	ffs_rw(um, fs->fs_csaddr, 0, fs->fs_csp, fs->fs_cssize, true, sync);
	fs->fs_fmod = 0;
	fs->fs_time = (int64_t)(realtime_ns() / 1000000000UL);
	dfs = kmalloc(fs->fs_sbsize);
	if (dfs == NULL)
		return;
	memcpy(dfs, fs, fs->fs_sbsize);
	memset(dfs->fs_ocsp, 0, sizeof(dfs->fs_ocsp));
	dfs->fs_contigdirs = NULL;
	dfs->fs_csp = NULL;
	dfs->fs_maxcluster = NULL;
	dfs->fs_active = NULL;
	dfs->fs_ronly = 0;
	ffs_rw(um, 0, (uint64_t)fs->fs_sblockloc, dfs, fs->fs_sbsize, true, sync);
	kfree(dfs);
}

/* ---- inodes ---- */

static void
set_rdev(struct inode *ip)
{
	uint32_t v = (uint32_t)DINO(ip)->di_rdev;

	ip->rdev = MKDEV((v >> 8) & 0xff, (v & 0xff) | ((v & 0xffff0000U) >> 8));
}

static int
ffs_read_inode(struct inode *ip)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct ffs_ino *fi;
	struct ufs2_dinode *d;

	if (ip->ino < ROOTINO || ip->ino >= (ino_t)fs->fs_ncg * fs->fs_ipg)
		return -EIO;
	fi = kzalloc(sizeof(*fi));
	if (fi == NULL)
		return -ENOMEM;
	d = &fi->i_din;
	ffs_rw(um, ino_to_fsba(fs, (uint32_t)ip->ino),
	    (uint64_t)ino_to_fsbo(fs, (uint32_t)ip->ino) * sizeof(*d), d,
	    sizeof(*d), false, false);
	if (d->di_mode == 0 && d->di_nlink == 0) {
		kfree(fi);
		return -ESTALE;
	}
	ip->priv = fi;
	ip->mode = d->di_mode;
	ip->nlink = (uint16_t)d->di_nlink;
	ip->uid = d->di_uid;
	ip->gid = d->di_gid;
	ip->size = d->di_size;
	ip->blocks = d->di_blocks;
	ip->atime.sec = d->di_atime;
	ip->atime.nsec = d->di_atimensec;
	ip->mtime.sec = d->di_mtime;
	ip->mtime.nsec = d->di_mtimensec;
	ip->ctime.sec = d->di_ctime;
	ip->ctime.nsec = d->di_ctimensec;
	if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode))
		set_rdev(ip);
	ffs_set_ops(ip);
	return 0;
}

/*
 * Write the inode back, to the device now (sync) or later.
 */
int
ffs_update(struct inode *ip, bool sync)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct ufs2_dinode *d = DINO(ip);

	if (ip->sb->rdonly)
		return 0;
	d->di_mode = (uint16_t)ip->mode;
	d->di_nlink = (int16_t)ip->nlink;
	d->di_uid = ip->uid;
	d->di_gid = ip->gid;
	d->di_size = ip->size;
	d->di_blocks = ip->blocks;
	d->di_atime = ip->atime.sec;
	d->di_atimensec = (int32_t)ip->atime.nsec;
	d->di_mtime = ip->mtime.sec;
	d->di_mtimensec = (int32_t)ip->mtime.nsec;
	d->di_ctime = ip->ctime.sec;
	d->di_ctimensec = (int32_t)ip->ctime.nsec;
	if (S_ISCHR(ip->mode) || S_ISBLK(ip->mode)) {
		unsigned ma = MAJOR(ip->rdev), mi = MINOR(ip->rdev);

		/* as BSD's makedev(): major 8 bits, minor 8 + 16 more */
		d->di_rdev = ((ma << 8) & 0x0000ff00U) | (mi & 0xff) |
		    ((mi << 8) & 0xffff0000U);
	}
	ffs_rw(um, ino_to_fsba(fs, (uint32_t)ip->ino),
	    (uint64_t)ino_to_fsbo(fs, (uint32_t)ip->ino) * sizeof(*d), d,
	    sizeof(*d), true, sync);
	ip->dirty = false;
	return 0;
}

static int
ffs_write_inode(struct inode *ip)
{
	return ffs_update(ip, false);
}

static void
ffs_put(struct inode *ip)
{
	kfree(ip->priv);
}

/*
 * The last reference to an unlinked inode: free its blocks, then the
 * inode.  (OpenBSD's ffs_inactive.)
 */
static void
ffs_evict(struct inode *ip)
{
	struct ufs2_dinode *d = DINO(ip);
	mode_t mode = ip->mode;

	if (ip->sb->rdonly)
		return;
	if (!(S_ISLNK(mode) && ip->size < (uint64_t)ITOFS(ip)->fs_maxsymlinklen) &&
	    !S_ISCHR(mode) && !S_ISBLK(mode))
		ffs_truncate(ip, 0);
	memset(d->di_db, 0, sizeof(d->di_db));
	memset(d->di_ib, 0, sizeof(d->di_ib));
	d->di_flags = 0;
	ip->size = 0;
	ip->blocks = 0;
	ip->mode = 0;
	ip->nlink = 0;
	ffs_update(ip, true);
	ffs_freefile(ip, (ino_t)ip->ino, mode);
}

/* ---- the file system ---- */

static int
ffs_statfs(struct super *sb, struct statfs64k *st)
{
	struct fs *fs = VFSTOFFS(sb)->um_fs;
	int64_t avail = freespace(fs, fs->fs_minfree);

	st->bsize = fs->fs_bsize;
	st->frsize = fs->fs_fsize;
	st->blocks = fs->fs_dsize;
	st->bfree = blkstofrags(fs, fs->fs_cstotal.cs_nbfree) +
	    fs->fs_cstotal.cs_nffree;
	st->bavail = avail > 0 ? (uint64_t)avail : 0;
	st->files = (uint64_t)fs->fs_ncg * fs->fs_ipg - ROOTINO;
	st->ffree = fs->fs_cstotal.cs_nifree;
	st->namelen = MAXNAMLEN;
	return 0;
}

/*
 * What the VFS does for a mount: sync_all() calls this every few seconds
 * and after a remount, which only changes sb->rdonly.  So this is where
 * the file system is marked in use when it becomes writable and clean when
 * it becomes read-only.
 */
static void
ffs_sync(struct super *sb)
{
	struct ffsmount *um = VFSTOFFS(sb);
	struct fs *fs = um->um_fs;

	if (um->um_damaged && !sb->rdonly) {
		printf("ffs: %s: damaged, stays read-only: unmount it and run fsck_ffs\n",
		    um->um_bdev->name);
		sb->rdonly = true;
	}
	if (!sb->rdonly && !um->um_rw) {
		if (um->um_unclean || um->um_damaged || fs->fs_contigsumsize > 0) {
			printf("ffs: %s: not clean, stays read-only: check it with fsck_ffs\n",
			    sb->bdev ? sb->bdev->name : "?");
			sb->rdonly = true;
			return;
		}
		fs->fs_clean = 0;
		fs->fs_fmod = 1;
		um->um_rw = true;
		ffs_sbupdate(um, true);
		return;
	}
	if (sb->rdonly && um->um_rw) {
		bsync_atomic(um->um_bdev);
		if (!um->um_damaged) {
			fs->fs_clean = 1;
			ffs_sbupdate(um, true);
			bsync_atomic(um->um_bdev);
		}
		um->um_rw = false;
		return;
	}
	if (um->um_rw && fs->fs_fmod)
		ffs_sbupdate(um, false);
}

/* power off or reboot: all written, marked clean; the mount stays */
static void
ffs_shutdown(struct super *sb)
{
	struct ffsmount *um = VFSTOFFS(sb);

	if (!um->um_rw)
		return;
	if (!um->um_damaged) {
		um->um_fs->fs_clean = 1;
		ffs_sbupdate(um, false);
	}
	bsync_atomic(um->um_bdev);
}

static void
ffs_umount(struct super *sb)
{
	struct ffsmount *um = VFSTOFFS(sb);
	struct fs *fs = um->um_fs;

	if (um->um_rw && !um->um_damaged) {
		fs->fs_clean = 1;
		ffs_sbupdate(um, true);
	}
	bsync_atomic(um->um_bdev);
	kfree(fs->fs_contigdirs);
	kfree(fs->fs_csp);
	kfree(fs);
	kfree(um);
	kfree(sb);
}

/*
 * Is this a super-block we can use, on a device of that many sectors?
 * (OpenBSD's ffs_validate(), with the checks its mount relies on others
 * for: every number that sizes a buffer or indexes memory is checked.)
 */
static bool
ffs_validate(struct fs *fs, uint64_t sectors)
{
	if (fs->fs_magic != FS_UFS2_MAGIC)
		return false;
	if ((uint32_t)fs->fs_bsize < MINBSIZE || (uint32_t)fs->fs_bsize > 65536 ||
	    (fs->fs_bsize & (fs->fs_bsize - 1)))
		return false;
	if (fs->fs_fsize < 512 || fs->fs_fsize > fs->fs_bsize ||
	    (fs->fs_fsize & (fs->fs_fsize - 1)))
		return false;
	if (fs->fs_frag != fs->fs_bsize / fs->fs_fsize || fs->fs_frag > MAXFRAG ||
	    fragtbl[fs->fs_frag] == NULL)
		return false;
	if ((1 << fs->fs_bshift) != fs->fs_bsize || (1 << fs->fs_fshift) != fs->fs_fsize ||
	    (1 << fs->fs_fragshift) != fs->fs_frag ||
	    (512 << fs->fs_fsbtodb) != fs->fs_fsize)
		return false;
	if ((uint32_t)fs->fs_sbsize > SBSIZE || (uint32_t)fs->fs_sbsize < sizeof(struct fs))
		return false;
	if (fs->fs_inodefmt != FS_44INODEFMT || fs->fs_maxsymlinklen <= 0 ||
	    fs->fs_maxsymlinklen > (int)MAXSYMLINKLEN_UFS2)
		return false;
	if (fs->fs_ncg == 0 || fs->fs_fpg <= 0 || fs->fs_fpg % fs->fs_frag ||
	    fs->fs_ipg == 0 || fs->fs_inopb != (uint32_t)fs->fs_bsize / sizeof(struct ufs2_dinode) ||
	    fs->fs_ipg % fs->fs_inopb)
		return false;
	if (fs->fs_nindir != fs->fs_bsize / (int)sizeof(int64_t))
		return false;
	if (fs->fs_cgsize < (int)sizeof(struct cg) || fs->fs_cgsize > fs->fs_bsize ||
	    fs->fs_cgsize % fs->fs_fsize)
		return false;
	if (fs->fs_cssize < (int)(fs->fs_ncg * sizeof(struct csum)) ||
	    fs->fs_cssize % fs->fs_fsize || fs->fs_cssize > (int)(fs->fs_ncg * sizeof(struct csum) + fs->fs_fsize))
		return false;
	if (fs->fs_size <= 0 || fs->fs_dsize <= 0 || fs->fs_dsize > fs->fs_size ||
	    (uint64_t)fs->fs_size * fs->fs_fsize > sectors * 512)
		return false;
	if (fs->fs_size > (int64_t)fs->fs_ncg * fs->fs_fpg ||
	    fs->fs_size <= (int64_t)(fs->fs_ncg - 1) * fs->fs_fpg)
		return false;
	if (fs->fs_sblkno <= 0 || fs->fs_cblkno <= 0 || fs->fs_iblkno <= 0 ||
	    fs->fs_dblkno <= 0 || fs->fs_dblkno >= fs->fs_fpg ||
	    fs->fs_csaddr <= 0 || fs->fs_csaddr >= fs->fs_size)
		return false;
	if (fs->fs_sblockloc != SBLOCK_UFS2 || fs->fs_minfree < 0 || fs->fs_minfree > 99 ||
	    fs->fs_optim < 0 || fs->fs_optim > 1)
		return false;
	return true;
}

static struct super *
ffs_mount(struct blockdev *b, int flags, const char *data)
{
	struct ffsmount *um;
	struct fs *fs;
	struct super *sb;
	struct inode *root;
	uint8_t *raw;
	bool ronly = (flags & MS_RDONLY) != 0;

	(void)data;
	raw = kmalloc(SBSIZE);
	if (raw == NULL)
		return ERR_PTR(-ENOMEM);
	if (b->read(b, SBLOCK_UFS2 / 512, SBSIZE / 512, raw)) {
		kfree(raw);
		return ERR_PTR(-EIO);
	}
	if (!ffs_validate((struct fs *)raw, b->sectors)) {
		kfree(raw);
		return ERR_PTR(-EINVAL);
	}
	um = kzalloc(sizeof(*um));
	if (um == NULL) {
		kfree(raw);
		return ERR_PTR(-ENOMEM);
	}
	um->um_bdev = b;
	um->um_fs = fs = kmalloc(((struct fs *)raw)->fs_sbsize);
	memcpy(fs, raw, ((struct fs *)raw)->fs_sbsize);
	kfree(raw);
	fs->fs_csp = kmalloc(fs->fs_cssize);
	fs->fs_contigdirs = kzalloc(fs->fs_ncg);
	fs->fs_maxcluster = NULL;
	fs->fs_active = NULL;
	if (fs->fs_csp == NULL || fs->fs_contigdirs == NULL)
		goto nomem;
	ffs_rw(um, fs->fs_csaddr, 0, fs->fs_csp, fs->fs_cssize, false, false);
	fs->fs_fmod = 0;
	fs->fs_ronly = 0;
	if (fs->fs_clean == 0) {
		um->um_unclean = true;
		printf("ffs: %s was not unmounted cleanly: check it with fsck_ffs\n",
		    b->name);
		if (!ronly)
			printf("ffs: %s: mounted read-only\n", b->name);
	}
	if (fs->fs_contigsumsize > 0) {
		printf("ffs: %s: cluster maps (contigsumsize %d) are not maintained: mounted read-only\n",
		    b->name, fs->fs_contigsumsize);
		um->um_unclean = true;
	}
	um->um_maxfilesize = MIN(fs->fs_maxfilesize, FS_KERNMAXFILESIZE(PAGE_SIZE, fs));
	ronly = ronly || um->um_unclean;

	sb = super_alloc(find_fs("ffs"), b);
	if (sb == NULL)
		goto nomem;
	sb->priv = um;
	sb->ops = &ffs_sops;
	sb->blocksize = fs->fs_bsize;
	sb->magic = FS_UFS2_MAGIC;
	sb->rdonly = ronly;
	um->um_sb = sb;
	root = iget(sb, ROOTINO);
	if (IS_ERR(root) || !S_ISDIR(root->mode)) {
		if (!IS_ERR(root))
			iput(root);
		kfree(sb);
		goto bad;
	}
	sb->root = root;
	if (!ronly) {
		fs->fs_clean = 0;
		fs->fs_fmod = 1;
		um->um_rw = true;
		ffs_sbupdate(um, true);
	}
	return sb;

nomem:
	kfree(fs->fs_csp);
	kfree(fs->fs_contigdirs);
	kfree(fs);
	kfree(um);
	return ERR_PTR(-ENOMEM);
bad:
	kfree(fs->fs_csp);
	kfree(fs->fs_contigdirs);
	kfree(fs);
	kfree(um);
	return ERR_PTR(-EIO);
}

static const struct super_ops ffs_sops = {
	.read_inode = ffs_read_inode,
	.write_inode = ffs_write_inode,
	.evict = ffs_evict,
	.put = ffs_put,
	.statfs = ffs_statfs,
	.sync = ffs_sync,
	.umount = ffs_umount,
	.shutdown = ffs_shutdown,
};

static struct fs_type ffs_type = {.name = "ffs", .needs_dev = true, .mount = ffs_mount};

void
ffs_register(void)
{
	register_fs(&ffs_type);
}

/*	$OpenBSD: ufs_lookup.c,v 1.61 2024/02/03 18:51:58 beck Exp $	*/
/*	$NetBSD: ufs_lookup.c,v 1.7 1996/02/09 22:36:06 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_lookup.c	8.9 (Berkeley) 8/11/94
 */

/*	$OpenBSD: ufs_vnops.c,v 1.166 2026/11/09 08:34:28 kirill Exp $	*/
/*	$NetBSD: ufs_vnops.c,v 1.18 1996/05/11 18:28:04 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 */

/*
 * Adapted for V86BSD from OpenBSD sys/ufs/ufs/ufs_lookup.c (revision 1.61), sys/ufs/ufs/ufs_vnops.c (revision 1.166).
 * Changes: the directory functions (ufs_lookup's search for room, ufs_direnter,
 *   ufs_dirremove, ufs_dirrewrite, ufs_dirempty) and the namespace operations (create,
 *   mkdir, link, remove, rmdir, rename, symlink, readlink, read, write) on V86BSD's
 *   inode_ops and file_ops: FFS2 only, no dirhash, a directory is not shortened when its
 *   last chunks are empty; the order of the writes (inode before the entry that names
 *   it, entry removed before the inode is changed) is kept, all with synchronous writes
 *   of the inodes and directory chunks; otherwise as OpenBSD.
 */

#include <ufs/ffs/ffs_vfs.h>

extern const struct file_ops fifo_fops, special_fops;

#define UFS_LINK_MAX	32767	/* di_nlink is 16 bits, signed */

void
ffs_set_ops(struct inode *ip)
{
	switch (ip->mode & S_IFMT) {
	case S_IFDIR:
		ip->iops = &ffs_dir_iops;
		ip->fops = &ffs_dir_fops;
		break;
	case S_IFREG:
		ip->iops = &ffs_file_iops;
		ip->fops = &ffs_file_fops;
		break;
	case S_IFLNK:
		ip->iops = &ffs_link_iops;
		ip->fops = NULL;
		break;
	case S_IFIFO:
		ip->iops = &ffs_special_iops;
		ip->fops = &fifo_fops;
		break;
	default:
		ip->iops = &ffs_special_iops;
		ip->fops = &special_fops;	/* devices and sockets */
		break;
	}
}

/*
 * ---- file data ----
 *
 * A file's block is fs_bsize bytes (or the fragments of the file's last
 * block) at the fragment address ffs_bmap() gives; its contents are in
 * the buffers of the fragments in turn.  Where the page cache holds a
 * page, that page has the file's current contents: read() takes them from
 * it, write() updates it as well as the blocks.  No chunk crosses a page,
 * a fragment or a block.
 */

/* the number of bytes from pos on that stay within one page and fragment */
static size_t
chunk_at(struct fs *fs, uint64_t pos, size_t n)
{
	size_t c = MIN(n, (size_t)(fs->fs_fsize - fragoff(fs, pos)));

	return MIN(c, (size_t)(PAGE_SIZE - (pos & (PAGE_SIZE - 1))));
}

/* the page cache's access to the blocks */
static int
ffs_rawio(struct inode *ip, void *buf, size_t n, uint64_t pos, bool write)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	char *k = buf;

	if (write && ip->sb->rdonly)
		return -EROFS;
	while (n) {
		daddr_t lbn = lblkno(fs, pos), bn;
		size_t seg = MIN(n, (size_t)(fs->fs_bsize - blkoff(fs, pos)));
		int err;

		if (write)
			err = ffs_balloc(ip, pos, (int)seg, &bn);
		else
			err = ffs_bmap(ip, lbn, &bn);
		if (err)
			return err;
		if (bn == 0)
			memset(k, 0, seg);
		else
			ffs_rw(um, bn, blkoff(fs, pos), k, seg, write, false);
		k += seg, pos += seg, n -= seg;
	}
	return 0;
}

static ssize_t
ffs_read(struct file *f, char *buf, size_t n, off_t *pos)
{
	struct inode *ip = f->ino;
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	size_t done = 0;
	daddr_t clbn = -1, cbn = 0;

	if (*pos < 0)
		return -EINVAL;
	if ((uint64_t)*pos >= ip->size)
		return 0;
	if (n > ip->size - (uint64_t)*pos)
		n = (size_t)(ip->size - (uint64_t)*pos);
	while (done < n) {
		uint64_t p = (uint64_t)*pos;
		size_t chunk = chunk_at(fs, p, n - done);
		daddr_t lbn = lblkno(fs, p);
		char *cp = pcache_lookup(ip, p);
		int r;

		if (cp)
			r = copy_to_user(buf + done, cp + (p & (PAGE_SIZE - 1)), chunk);
		else {
			if (lbn != clbn) {
				int err = ffs_bmap(ip, lbn, &cbn);

				if (err)
					return done ? (ssize_t)done : err;
				clbn = lbn;
			}
			if (cbn == 0)
				r = clear_user(buf + done, chunk);
			else {
				struct buf *bp = bread(um->um_bdev,
				    (uint64_t)(cbn + numfrags(fs, blkoff(fs, p))), fs->fs_fsize);

				r = copy_to_user(buf + done, bp->data + fragoff(fs, p), chunk);
				brelse(bp);
			}
		}
		if (r)
			return done ? (ssize_t)done : -EFAULT;
		done += chunk;
		*pos += (off_t)chunk;
	}
	if (!(f->flags & O_NOATIME) && !ip->sb->rdonly) {
		/* relatime-free: POSIX atime */
		uint64_t ns = realtime_ns();

		ip->atime.sec = (int64_t)(ns / 1000000000UL);
		ip->atime.nsec = (int64_t)(ns % 1000000000UL);
		ip->dirty = true;
	}
	return (ssize_t)done;
}

static ssize_t
ffs_write(struct file *f, const char *buf, size_t n, off_t *pos)
{
	struct inode *ip = f->ino;
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	size_t done = 0;
	int err = 0;
	char *tmp;

	if (ip->sb->rdonly)
		return -EROFS;
	if (f->flags & O_APPEND)
		*pos = (off_t)ip->size;
	if (*pos < 0)
		return -EINVAL;
	{
		uint64_t lim = cur_proc()->rlim[RLIMIT_FSIZE].cur;

		if ((uint64_t)*pos >= lim && n) {
			signal_simple(cur_proc(), SIGXFSZ);
			return -EFBIG;
		}
		if ((uint64_t)*pos + n > lim)
			n = (size_t)(lim - (uint64_t)*pos);
	}
	if ((uint64_t)*pos >= um->um_maxfilesize && n)
		return -EFBIG;
	if ((uint64_t)*pos + n > um->um_maxfilesize)
		n = (size_t)(um->um_maxfilesize - (uint64_t)*pos);
	if (n == 0)
		return 0;
	tmp = kmalloc((size_t)fs->fs_bsize);
	if (tmp == NULL)
		return -ENOMEM;
	while (done < n) {
		uint64_t p = (uint64_t)*pos;
		size_t seg = MIN(n - done, (size_t)(fs->fs_bsize - blkoff(fs, p)));
		daddr_t bn;
		size_t k;

		/* what is written is fetched before anything is allocated */
		if (copy_from_user(tmp, buf + done, seg)) {
			err = -EFAULT;
			break;
		}
		err = ffs_balloc(ip, p, (int)seg, &bn);
		if (err)
			break;
		if (p + seg > ip->size) {
			ip->size = p + seg;
			pcache_resize(ip);
		}
		ffs_rw(um, bn, blkoff(fs, p), tmp, seg, true, false);
		for (k = 0; k < seg;) {
			size_t c = MIN(seg - k, (size_t)(PAGE_SIZE - ((p + k) & (PAGE_SIZE - 1))));
			char *cp = pcache_lookup(ip, p + k);

			if (cp)
				memcpy(cp + ((p + k) & (PAGE_SIZE - 1)), tmp + k, c);
			k += c;
		}
		done += seg;
		*pos += (off_t)seg;
	}
	kfree(tmp);
	if (done) {
		inode_set_times(ip, false, true, true);
		/* writing clears set-user-ID (not for root) */
		if ((ip->mode & (S_ISUID | S_ISGID)) && cur_proc()->cred.fsuid != 0) {
			ip->mode &= ~S_ISUID;
			if (ip->mode & 010)
				ip->mode &= ~S_ISGID;
		}
		if (f->flags & O_SYNC) {
			ffs_update(ip, false);
			bsync(um->um_bdev);
		}
		return (ssize_t)done;
	}
	return err;
}

static int
ffs_fsync(struct file *f)
{
	struct inode *ip = f->ino;
	struct ffsmount *um = ITOUM(ip);
	int r = pcache_sync(ip);

	if (r)
		return r;
	if (ip->dirty)
		ffs_update(ip, false);
	if (um->um_rw && um->um_fs->fs_fmod)
		ffs_sbupdate(um, false);
	bsync(um->um_bdev);
	return 0;
}

static int
ffs_iop_truncate(struct inode *ip, uint64_t size)
{
	if (ip->sb->rdonly)
		return -EROFS;
	return ffs_truncate(ip, size);
}

/*
 * ---- directories ----
 *
 * A directory is a number of chunks of DIRBLKSIZ (512) bytes, with
 * entries (struct direct) that do not cross a chunk; the fragment size is
 * a multiple of that, so a chunk is always within one fragment's buffer.
 * Entries are changed in the buffer and written to the device at once.
 */

/* the buffer of the fragment of directory dp that holds the byte at off; NULL in a hole */
static struct buf *
dir_buf(struct inode *dp, uint64_t off)
{
	struct ffsmount *um = ITOUM(dp);
	struct fs *fs = um->um_fs;
	daddr_t bn;

	if (ffs_bmap(dp, lblkno(fs, off), &bn) || bn == 0)
		return NULL;
	return bread(um->um_bdev, (uint64_t)(bn + numfrags(fs, blkoff(fs, off))), fs->fs_fsize);
}

/* write a directory buffer to the device now, and release it */
static void
dir_bwrite(struct buf *bp)
{
	bwritenow(bp);
	brelse(bp);
}

/* is the entry at ep, in a chunk with `left' bytes from it on, sane? */
static bool
dirent_ok(const struct direct *ep, unsigned left)
{
	unsigned rl = ep->d_reclen;

	if (rl < 8 || (rl & 3) || rl > left)
		return false;
	if (ep->d_ino != 0 && (ep->d_namlen == 0 || DIRSIZ(ep) > rl))
		return false;
	return true;
}

static void
dirent_fill(struct direct *ep, ino_t ino, unsigned reclen, mode_t mode,
    const char *name, size_t len)
{
	ep->d_ino = (uint32_t)ino;
	ep->d_reclen = (uint16_t)reclen;
	ep->d_type = (uint8_t)IFTODT(mode);
	ep->d_namlen = (uint8_t)len;
	memset(ep->d_name, 0, DIRECTSIZ(len) - offsetof(struct direct, d_name));
	memcpy(ep->d_name, name, len);
}

/*
 * Walk the entries from byte offset start on; fn returns true to stop.
 * It is given the buffer, the offset of the entry in it, and its offset in
 * the directory.  A chunk that is damaged is skipped.
 */
typedef bool (*dir_fn)(struct buf *bp, unsigned off, uint64_t pos, void *arg);

static int
dir_walk(struct inode *dp, uint64_t start, dir_fn fn, void *arg)
{
	for (uint64_t cpos = start - start % DIRBLKSIZ; cpos < dp->size; cpos += DIRBLKSIZ) {
		struct buf *bp = dir_buf(dp, cpos);
		unsigned base, off = 0;

		if (bp == NULL)
			continue;
		base = (unsigned)(cpos % ITOFS(dp)->fs_fsize);
		while (off < DIRBLKSIZ) {
			struct direct *ep = (struct direct *)(bp->data + base + off);

			if (!dirent_ok(ep, DIRBLKSIZ - off))
				break;
			if (cpos + off >= start && fn(bp, base + off, cpos + off, arg)) {
				brelse(bp);
				return 1;
			}
			off += ep->d_reclen;
		}
		brelse(bp);
	}
	return 0;
}

struct find_arg {
	const char *name;
	size_t len;
	uint32_t ino;
	uint64_t pos;
};

static bool
find_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg)
{
	struct find_arg *a = arg;
	struct direct *ep = (struct direct *)(bp->data + off);

	if (ep->d_ino != 0 && ep->d_namlen == a->len && !memcmp(ep->d_name, a->name, a->len)) {
		a->ino = ep->d_ino;
		a->pos = pos;
		return true;
	}
	return false;
}

static struct inode *
ffs_lookup(struct inode *dir, const char *name, size_t len)
{
	struct find_arg a = {name, len, 0, 0};

	if (len > MAXNAMLEN)
		return ERR_PTR(-ENAMETOOLONG);
	if (!dir_walk(dir, 0, find_fn, &a))
		return ERR_PTR(-ENOENT);
	return iget(dir->sb, a.ino);
}

struct readdir_arg {
	struct dir_ctx *ctx;
	struct file *f;
};

static bool
readdir_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg)
{
	struct readdir_arg *a = arg;
	struct direct *ep = (struct direct *)(bp->data + off);
	uint64_t next = pos + ep->d_reclen;

	if (ep->d_ino != 0) {
		if (!a->ctx->fill(a->ctx, ep->d_name, ep->d_namlen, ep->d_ino,
		    ep->d_type, (off_t)next))
			return true;
	}
	a->f->pos = (off_t)next;
	return false;
}

static int
ffs_readdir(struct file *f, struct dir_ctx *ctx)
{
	struct readdir_arg a = {ctx, f};

	dir_walk(f->ino, (uint64_t)f->pos, readdir_fn, &a);
	return 0;
}

/*
 * Enter a name in a directory (OpenBSD's ufs_lookup() finding the space
 * and ufs_direnter() using it): in the slack of an entry, or in an unused
 * entry, of a chunk that has room; failing that, in a chunk made roomy by
 * moving its entries together; failing that, in a new chunk.
 */
static int
dir_add(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode)
{
	struct ffsmount *um = ITOUM(dir);
	struct fs *fs = um->um_fs;
	unsigned need = (unsigned)DIRECTSIZ(len);
	uint64_t cpos;
	daddr_t bn;
	int err;
	struct direct *np;
	char chunk[DIRBLKSIZ];

	for (cpos = 0; cpos < dir->size; cpos += DIRBLKSIZ) {
		struct buf *bp = dir_buf(dir, cpos);
		unsigned base, off = 0, total = 0;
		int found = -1;
		bool bad = false;

		if (bp == NULL)
			continue;
		base = (unsigned)(cpos % fs->fs_fsize);
		while (off < DIRBLKSIZ) {
			struct direct *ep = (struct direct *)(bp->data + base + off);
			unsigned slack;

			if (!dirent_ok(ep, DIRBLKSIZ - off)) {
				bad = true;
				break;
			}
			slack = ep->d_ino ? ep->d_reclen - (unsigned)DIRSIZ(ep) : ep->d_reclen;
			if (slack >= need && found < 0)
				found = (int)off;
			total += slack;
			off += ep->d_reclen;
		}
		if (bad || total < need) {
			brelse(bp);
			continue;
		}
		if (found >= 0) {
			struct direct *ep = (struct direct *)(bp->data + base + found);
			unsigned reclen = ep->d_reclen;

			np = ep;
			if (ep->d_ino) {
				unsigned dsize = (unsigned)DIRSIZ(ep);

				ep->d_reclen = (uint16_t)dsize;
				np = (struct direct *)((char *)ep + dsize);
				reclen -= dsize;
			}
			dirent_fill(np, ino, reclen, mode, name, len);
		} else {
			/* compact: the live entries together, the new one takes the rest */
			unsigned to = 0;

			memset(chunk, 0, sizeof(chunk));
			off = 0;
			while (off < DIRBLKSIZ) {
				struct direct *ep = (struct direct *)(bp->data + base + off);
				unsigned rl = ep->d_reclen;

				if (ep->d_ino != 0) {
					unsigned dsize = (unsigned)DIRSIZ(ep);

					memcpy(chunk + to, ep, dsize);
					((struct direct *)(chunk + to))->d_reclen = (uint16_t)dsize;
					to += dsize;
				}
				off += rl;
			}
			dirent_fill((struct direct *)(chunk + to), ino, DIRBLKSIZ - to,
			    mode, name, len);
			memcpy(bp->data + base, chunk, DIRBLKSIZ);
		}
		dir_bwrite(bp);
		inode_set_times(dir, false, true, true);
		return 0;
	}

	/* a new chunk at the end */
	err = ffs_balloc(dir, dir->size, DIRBLKSIZ, &bn);
	if (err)
		return err;
	memset(chunk, 0, sizeof(chunk));
	dirent_fill((struct direct *)chunk, ino, DIRBLKSIZ, mode, name, len);
	ffs_rw(um, bn, blkoff(fs, dir->size), chunk, DIRBLKSIZ, true, true);
	dir->size += DIRBLKSIZ;
	inode_set_times(dir, false, true, true);
	return 0;
}

/*
 * Take a name out of a directory (ufs_dirremove): the space goes to the
 * entry before it in the chunk, or, for the first entry, the entry is
 * only marked unused.
 */
static int
dir_remove(struct inode *dir, const char *name, size_t len)
{
	struct fs *fs = ITOFS(dir);

	for (uint64_t cpos = 0; cpos < dir->size; cpos += DIRBLKSIZ) {
		struct buf *bp = dir_buf(dir, cpos);
		unsigned base, off = 0, prev = ~0U;

		if (bp == NULL)
			continue;
		base = (unsigned)(cpos % fs->fs_fsize);
		while (off < DIRBLKSIZ) {
			struct direct *ep = (struct direct *)(bp->data + base + off);

			if (!dirent_ok(ep, DIRBLKSIZ - off))
				break;
			if (ep->d_ino != 0 && ep->d_namlen == len &&
			    !memcmp(ep->d_name, name, len)) {
				if (prev != ~0U) {
					struct direct *pp = (struct direct *)(bp->data + base + prev);

					pp->d_reclen += ep->d_reclen;
				} else
					ep->d_ino = 0;
				dir_bwrite(bp);
				inode_set_times(dir, false, true, true);
				return 0;
			}
			prev = off;
			off += ep->d_reclen;
		}
		brelse(bp);
	}
	return -ENOENT;
}

/* make an entry point to another inode (ufs_dirrewrite): rename over, ".." */
static int
dir_set(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode)
{
	struct find_arg a = {name, len, 0, 0};
	struct buf *bp;
	struct direct *ep;

	if (!dir_walk(dir, 0, find_fn, &a))
		return -ENOENT;
	bp = dir_buf(dir, a.pos);
	if (bp == NULL)
		return -EIO;
	ep = (struct direct *)(bp->data + a.pos % ITOFS(dir)->fs_fsize);
	ep->d_ino = (uint32_t)ino;
	ep->d_type = (uint8_t)IFTODT(mode);
	dir_bwrite(bp);
	inode_set_times(dir, false, true, true);
	return 0;
}

static bool
empty_fn(struct buf *bp, unsigned off, uint64_t pos, void *arg)
{
	struct direct *ep = (struct direct *)(bp->data + off);

	(void)pos;
	if (ep->d_ino == 0)
		return false;
	if ((ep->d_namlen == 1 && ep->d_name[0] == '.') ||
	    (ep->d_namlen == 2 && ep->d_name[0] == '.' && ep->d_name[1] == '.'))
		return false;
	*(bool *)arg = false;
	return true;
}

static bool
dir_empty(struct inode *dir)
{
	bool empty = true;

	dir_walk(dir, 0, empty_fn, &empty);
	return empty;
}

/*
 * ---- namespace operations ----
 */

/*
 * A new inode in the file system, as dir's: its number from the allocator,
 * its on-disk inode as the last file left it but for the generation number.
 */
static struct inode *
ffs_new_inode(struct inode *dir, mode_t mode)
{
	struct ffsmount *um = ITOUM(dir);
	struct fs *fs = um->um_fs;
	struct ffs_ino *fi;
	struct ufs2_dinode old;
	struct inode *ip;
	struct cred *c = &cur_proc()->cred;
	uint32_t gen;
	ino_t ino;
	int err;

	if (dir->sb->rdonly)
		return ERR_PTR(-EROFS);
	err = ffs_inode_alloc(dir, mode, &ino);
	if (err)
		return ERR_PTR(err);
	fi = kzalloc(sizeof(*fi));
	if (fi == NULL) {
		ffs_freefile(dir, ino, mode);
		return ERR_PTR(-ENOMEM);
	}
	ip = new_inode(dir->sb, ino);
	if (ip == NULL) {
		kfree(fi);
		ffs_freefile(dir, ino, mode);
		return ERR_PTR(-ENOMEM);
	}
	ip->priv = fi;
	ffs_rw(um, ino_to_fsba(fs, (uint32_t)ino),
	    (uint64_t)ino_to_fsbo(fs, (uint32_t)ino) * sizeof(old), &old,
	    sizeof(old), false, false);
	/*
	 * Set up a new generation number for this inode.
	 * On wrap, we make sure to assign a number != 0 and != UINT_MAX
	 * (the original value).
	 */
	gen = (uint32_t)old.di_gen;
	if (gen != 0)
		gen++;
	while (gen == 0)
		gen = ffs_random(0xffffffffU);
	fi->i_din.di_gen = (int32_t)gen;
	ip->mode = mode;
	ip->uid = c->fsuid;
	ip->gid = (dir->mode & S_ISGID) ? dir->gid : c->fsgid;
	if ((dir->mode & S_ISGID) && S_ISDIR(mode))
		ip->mode |= S_ISGID;
	ip->nlink = 1;
	inode_set_times(ip, true, true, true);
	fi->i_din.di_birthtime = ip->ctime.sec;
	fi->i_din.di_birthnsec = (int32_t)ip->ctime.nsec;
	ffs_set_ops(ip);
	return ip;
}

static struct inode *
ffs_create(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t rdev)
{
	struct ffsmount *um = ITOUM(dir);
	struct fs *fs = um->um_fs;
	struct inode *ip;
	int r;

	if (len > MAXNAMLEN)
		return ERR_PTR(-ENAMETOOLONG);
	if (S_ISDIR(mode) && dir->nlink >= UFS_LINK_MAX)
		return ERR_PTR(-EMLINK);
	ip = ffs_new_inode(dir, mode);
	if (IS_ERR(ip))
		return ip;
	ip->rdev = rdev;
	if (S_ISDIR(mode)) {
		/* "." and "..": the first chunk, written before the entry that links the directory in */
		char chunk[DIRBLKSIZ];
		struct direct *dp = (struct direct *)chunk;
		daddr_t bn;

		r = ffs_balloc(ip, 0, DIRBLKSIZ, &bn);
		if (r) {
			ip->nlink = 0;
			iput(ip);
			return ERR_PTR(r);
		}
		memset(chunk, 0, sizeof(chunk));
		dirent_fill(dp, ip->ino, DIRECTSIZ(1), S_IFDIR, ".", 1);
		dirent_fill((struct direct *)(chunk + DIRECTSIZ(1)), dir->ino,
		    DIRBLKSIZ - DIRECTSIZ(1), S_IFDIR, "..", 2);
		ffs_rw(um, bn, 0, chunk, DIRBLKSIZ, true, true);
		ip->size = DIRBLKSIZ;
		ip->nlink = 2;
	}
	ffs_update(ip, true);
	r = dir_add(dir, name, len, ip->ino, mode);
	if (r) {
		ip->nlink = 0;
		iput(ip);
		return ERR_PTR(r);
	}
	if (S_ISDIR(mode))
		dir->nlink++;
	ffs_update(dir, true);
	(void)fs;
	return ip;
}

static int
ffs_link(struct inode *dir, const char *name, size_t len, struct inode *ip)
{
	int r;

	if (dir->sb->rdonly)
		return -EROFS;
	if (len > MAXNAMLEN)
		return -ENAMETOOLONG;
	if (ip->nlink >= UFS_LINK_MAX)
		return -EMLINK;
	ip->nlink++;
	inode_set_times(ip, false, false, true);
	ffs_update(ip, true);
	r = dir_add(dir, name, len, ip->ino, ip->mode);
	if (r) {
		ip->nlink--;
		ffs_update(ip, true);
		return r;
	}
	ffs_update(dir, true);
	return 0;
}

static int
ffs_unlink(struct inode *dir, const char *name, size_t len, struct inode *ip)
{
	int r;

	if (dir->sb->rdonly)
		return -EROFS;
	r = dir_remove(dir, name, len);
	if (r)
		return r;
	ip->nlink--;
	inode_set_times(ip, false, false, true);
	ffs_update(dir, true);
	return 0;
}

static int
ffs_rmdir(struct inode *dir, const char *name, size_t len, struct inode *ip)
{
	int r;

	if (dir->sb->rdonly)
		return -EROFS;
	if (!dir_empty(ip))
		return -ENOTEMPTY;
	r = dir_remove(dir, name, len);
	if (r)
		return r;
	ip->nlink = 0;
	dir->nlink--;
	ip->dirty = true;
	inode_set_times(dir, false, true, true);
	ffs_update(dir, true);
	return 0;
}

static int
ffs_rename(struct inode *odir, const char *oname, size_t olen, struct inode *ip,
    struct inode *ndir, const char *nname, size_t nlen, struct inode *target)
{
	bool isdir = S_ISDIR(ip->mode);

	if (odir->sb->rdonly)
		return -EROFS;
	if (nlen > MAXNAMLEN)
		return -ENAMETOOLONG;
	if (target) {
		int r;

		if (isdir && !dir_empty(target))
			return -ENOTEMPTY;
		r = dir_set(ndir, nname, nlen, ip->ino, ip->mode);
		if (r)
			return r;
		if (isdir) {
			target->nlink = 0;
			ndir->nlink--;
		} else
			target->nlink--;
		inode_set_times(target, false, false, true);
	} else {
		int r;

		if (isdir && ndir != odir && ndir->nlink >= UFS_LINK_MAX)
			return -EMLINK;
		r = dir_add(ndir, nname, nlen, ip->ino, ip->mode);
		if (r)
			return r;
	}
	dir_remove(odir, oname, olen);
	if (isdir && odir != ndir) {
		dir_set(ip, "..", 2, ndir->ino, S_IFDIR);
		odir->nlink--;
		ndir->nlink++;
	}
	inode_set_times(ip, false, false, true);
	ffs_update(odir, true);
	if (ndir != odir)
		ffs_update(ndir, true);
	ffs_update(ip, false);
	return 0;
}

static struct inode *
ffs_symlink(struct inode *dir, const char *name, size_t len, const char *target)
{
	struct ffsmount *um = ITOUM(dir);
	struct fs *fs = um->um_fs;
	size_t tl = strlen(target);
	struct inode *ip;
	int r;

	if (len > MAXNAMLEN || tl >= 4096)
		return ERR_PTR(-ENAMETOOLONG);
	ip = ffs_new_inode(dir, S_IFLNK | 0777);
	if (IS_ERR(ip))
		return ip;
	if (tl < (size_t)fs->fs_maxsymlinklen) {
		memcpy(DINO(ip)->di_shortlink, target, tl);
	} else {
		daddr_t bn;

		r = ffs_balloc(ip, 0, (int)tl, &bn);
		if (r) {
			ip->nlink = 0;
			iput(ip);
			return ERR_PTR(r);
		}
		ffs_rw(um, bn, 0, (void *)target, tl, true, false);
	}
	ip->size = tl;
	ffs_update(ip, true);
	r = dir_add(dir, name, len, ip->ino, ip->mode);
	if (r) {
		ip->nlink = 0;
		iput(ip);
		return ERR_PTR(r);
	}
	ffs_update(dir, true);
	return ip;
}

static ssize_t
ffs_readlink(struct inode *ip, char *buf, size_t n)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	size_t len = MIN((size_t)ip->size, n);
	daddr_t bn;

	if (ip->size < (uint64_t)fs->fs_maxsymlinklen) {
		memcpy(buf, DINO(ip)->di_shortlink, len);
		return (ssize_t)len;
	}
	if (ffs_bmap(ip, 0, &bn) || bn == 0)
		return -EIO;
	len = MIN(len, (size_t)fs->fs_bsize);
	ffs_rw(um, bn, 0, buf, len, false, false);
	return (ssize_t)len;
}

const struct inode_ops ffs_dir_iops = {
	.lookup = ffs_lookup,
	.create = ffs_create,
	.link = ffs_link,
	.unlink = ffs_unlink,
	.rmdir = ffs_rmdir,
	.rename = ffs_rename,
	.symlink = ffs_symlink,
};
const struct inode_ops ffs_file_iops = {.truncate = ffs_iop_truncate, .rawio = ffs_rawio};
const struct inode_ops ffs_link_iops = {.readlink = ffs_readlink};
const struct inode_ops ffs_special_iops = {.truncate = NULL};
const struct file_ops ffs_dir_fops = {.readdir = ffs_readdir, .fsync = ffs_fsync, .seekable = true};
const struct file_ops ffs_file_fops = {
	.read = ffs_read, .write = ffs_write, .fsync = ffs_fsync, .seekable = true};

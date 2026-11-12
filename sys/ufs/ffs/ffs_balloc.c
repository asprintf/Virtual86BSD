/*	$OpenBSD: ffs_balloc.c,v 1.47 2024/04/13 23:44:11 jsg Exp $	*/
/*	$NetBSD: ffs_balloc.c,v 1.3 1996/02/09 22:22:21 christos Exp $	*/

/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
 *
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 */

/*	$OpenBSD: ffs_inode.c,v 1.83 2024/02/03 18:51:58 beck Exp $	*/
/*	$NetBSD: ffs_inode.c,v 1.10 1996/05/11 18:27:19 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_inode.c	8.8 (Berkeley) 10/19/94
 */

/*	$OpenBSD: ufs_bmap.c,v 1.37 2021/12/12 09:14:59 visa Exp $	*/
/*	$NetBSD: ufs_bmap.c,v 1.3 1996/02/09 22:36:00 christos Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)ufs_bmap.c	8.6 (Berkeley) 1/21/94
 */

/*
 * Adapted for V86BSD from OpenBSD sys/ufs/ffs/ffs_balloc.c (revision 1.47), sys/ufs/ffs/ffs_inode.c (revision 1.83), sys/ufs/ufs/ufs_bmap.c (revision 1.37).
 * Changes: ffs2_balloc(), ffs_truncate() and ffs_indirtrunc() of FFS2, and ufs_getlbns(), on
 *   V86BSD's inode and buffer cache: no buffers (blocks are given by their address and
 *   read and written in fragment-sized buffers by ffs_rw), no quota, no soft updates,
 *   a failed allocation is undone from a list of what it allocated, the new blocks are
 *   zeroed when they are allocated; ffs_bmap() is ufs_bmaparray() without the cluster
 *   read-ahead; otherwise as OpenBSD.
 */

#include <ufs/ffs/ffs_vfs.h>

#define SINGLE	0	/* index of single indirect block */
#define DOUBLE	1	/* index of double indirect block */
#define TRIPLE	2	/* index of triple indirect block */

/* the pointers of a file as the inode keeps them */
#define DBPREV(din, n)	((n) > 0 ? (din)->di_db[(n) - 1] : 0)

/* a block pointer, 64 bits, entry idx of the indirect block at fragment bn */
static int64_t
rd64(struct ffsmount *um, daddr_t bn, int idx)
{
	int64_t v;

	ffs_rw(um, bn, (uint64_t)idx * sizeof(v), &v, sizeof(v), false, false);
	return v;
}

static void
wr64(struct ffsmount *um, daddr_t bn, int idx, int64_t v)
{
	ffs_rw(um, bn, (uint64_t)idx * sizeof(v), &v, sizeof(v), true, false);
}

/* is that a block address inside the file system? */
static bool
okblk(struct fs *fs, int64_t b)
{
	return b > 0 && b < fs->fs_size;
}

/*
 * The indirect blocks to logical block lbn (OpenBSD's ufs_getlbns): the
 * number of them (1, 2 or 3), off[0] the index of the first in the inode
 * (di_ib[off[0]]), off[1 .. n] the index in each indirect block on the
 * way, the last one the pointer to the data block.  -1: beyond what
 * triple indirection reaches.
 */
static int
ffs_getlbns(struct fs *fs, daddr_t lbn, int off[NIADDR + 1])
{
	int64_t nindir = NINDIR(fs), span = nindir;
	int level, l;

	lbn -= NDADDR;
	for (level = 1; level <= NIADDR; level++) {
		if (lbn < span)
			break;
		lbn -= span;
		span *= nindir;
	}
	if (level > NIADDR)
		return -1;
	off[0] = level - 1;
	for (l = level - 1; l >= 0; l--) {
		int64_t div = 1;
		int k;

		for (k = 0; k < l; k++)
			div *= nindir;
		off[level - l] = (int)(lbn / div);
		lbn %= div;
	}
	return level;
}

/*
 * The fragment address of block lbn of the file, *bnp, 0 where it is a
 * hole.
 */
int
ffs_bmap(struct inode *ip, daddr_t lbn, daddr_t *bnp)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct ufs2_dinode *din = DINO(ip);
	int off[NIADDR + 1], level, i;
	int64_t nb;

	*bnp = 0;
	if (lbn < 0)
		return -EFBIG;
	if (lbn < NDADDR) {
		nb = din->di_db[lbn];
		if (nb != 0 && !okblk(fs, nb))
			return -EIO;
		*bnp = nb;
		return 0;
	}
	level = ffs_getlbns(fs, lbn, off);
	if (level < 0)
		return -EFBIG;
	nb = din->di_ib[off[0]];
	for (i = 1; i <= level && nb != 0; i++) {
		if (!okblk(fs, nb))
			return -EIO;
		nb = rd64(um, nb, off[i]);
	}
	if (nb != 0 && !okblk(fs, nb))
		return -EIO;
	*bnp = nb;
	return 0;
}

/*
 * Balloc defines the structure of file system storage by allocating the
 * physical blocks given the inode and the logical block number in a file.
 * The file's contents from byte offset `off' for `size' bytes (within one
 * block) are to be written: the block is allocated if it is not, a block
 * that is a fragment (the end of the file) is made as big as that needs.
 * *bnp is the fragment address of the block.  Blocks (and what is added to
 * a fragment) are zeroed, and the indirect blocks that lead to the block
 * are written before they are linked in.
 *
 * (From OpenBSD's ffs2_balloc(): without the buffers, which the callers
 * here get from the address, and with its failure unwinding done from a
 * list of what was allocated.)
 */
int
ffs_balloc(struct inode *ip, uint64_t off, int size, daddr_t *bnp)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct ufs2_dinode *din = DINO(ip);
	daddr_t lbn, lastlbn, nb, newb, pref;
	int osize, nsize, error, i, level, offs[NIADDR + 1];
	struct {
		daddr_t blk;	/* what was allocated */
		daddr_t ind;	/* the indirect block its pointer is in, 0: the inode */
		int idx;	/* the index of the pointer */
	} alloc[NIADDR + 1];
	int nalloc = 0;
	daddr_t ind;

	*bnp = 0;
	lbn = lblkno(fs, off);
	size = (int)blkoff(fs, off) + size;
	if (size > fs->fs_bsize)
		return -EINVAL;
	if (lbn < 0)
		return -EFBIG;

	/*
	 * If the next write will extend the file into a new block, and the
	 * file is currently composed of a fragment, this fragment has to be
	 * extended to be a full block.
	 */
	lastlbn = lblkno(fs, ip->size);
	if (lastlbn < NDADDR && lastlbn < lbn) {
		nb = lastlbn;
		osize = (int)ffs_blksize(fs, ip->size, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = ffs_realloccg(ip, nb, ffs2_blkpref(ip,
			    lastlbn, (int)nb, DBPREV(din, nb)), osize,
			    (int)fs->fs_bsize, &newb);
			if (error)
				return error;

			ip->size = lblktosize(fs, nb + 1);
			pcache_resize(ip);
			din->di_db[nb] = newb;
			ip->dirty = true;
		}
	}

	/*
	 * The first NDADDR blocks are direct.
	 */
	if (lbn < NDADDR) {
		nb = din->di_db[lbn];

		if (nb != 0 && !okblk(fs, nb))
			return -EIO;
		if (nb != 0 && ip->size >= lblktosize(fs, lbn + 1)) {
			/*
			 * The direct block is already allocated and the file
			 * extends past this block, thus this must be a whole
			 * block.
			 */
			*bnp = nb;
			return 0;
		}

		if (nb != 0) {
			/*
			 * Consider the need to allocate a fragment.
			 */
			osize = (int)fragroundup(fs, blkoff(fs, ip->size));
			nsize = (int)fragroundup(fs, size);

			if (nsize <= osize) {
				/*
				 * The existing block is already at least as
				 * big as we want.
				 */
				*bnp = nb;
				return 0;
			} else {
				/*
				 * The existing block is smaller than we want,
				 * grow it.
				 */
				error = ffs_realloccg(ip, lbn,
				    ffs2_blkpref(ip, lbn, (int)lbn,
				    DBPREV(din, lbn)), osize, nsize, &newb);
				if (error)
					return error;
			}
		} else {
			/*
			 * The block was not previously allocated, allocate a
			 * new block or fragment.
			 */
			if (ip->size < lblktosize(fs, lbn + 1))
				nsize = (int)fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;

			error = ffs_alloc(ip, lbn, ffs2_blkpref(ip, lbn,
			    (int)lbn, DBPREV(din, lbn)), nsize, &newb);
			if (error)
				return error;
			ffs_rw(um, newb, 0, NULL, (size_t)nsize, true, false);
		}

		din->di_db[lbn] = newb;
		ip->dirty = true;
		*bnp = newb;
		return 0;
	}

	/*
	 * Determine the number of levels of indirection.
	 */
	level = ffs_getlbns(fs, lbn, offs);
	if (level < 0)
		return -EFBIG;

	/*
	 * Fetch the first indirect block allocating it necessary.
	 */
	pref = 0;
	nb = din->di_ib[offs[0]];
	if (nb != 0 && !okblk(fs, nb))
		return -EIO;
	if (nb == 0) {
		pref = ffs2_blkpref(ip, lbn, -offs[0] - 1, 0);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb);
		if (error)
			goto fail;

		nb = newb;
		alloc[nalloc].blk = nb;
		alloc[nalloc].ind = 0;
		alloc[nalloc++].idx = offs[0];
		/*
		 * Write synchronously so that indirect blocks never
		 * point at garbage.
		 */
		ffs_rw(um, nb, 0, NULL, (size_t)fs->fs_bsize, true, true);

		din->di_ib[offs[0]] = nb;
		ip->dirty = true;
	}

	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	ind = nb;
	for (i = 1;; i++) {
		nb = rd64(um, ind, offs[i]);
		if (nb != 0 && !okblk(fs, nb)) {
			error = -EIO;
			goto fail;
		}
		if (i == level)
			break;
		if (nb == 0) {
			if (pref == 0)
				pref = ffs2_blkpref(ip, lbn, i - level, 0);

			error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb);
			if (error)
				goto fail;

			nb = newb;
			alloc[nalloc].blk = nb;
			alloc[nalloc].ind = ind;
			alloc[nalloc++].idx = offs[i];
			ffs_rw(um, nb, 0, NULL, (size_t)fs->fs_bsize, true, true);
			wr64(um, ind, offs[i], nb);
		}
		ind = nb;
	}

	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		int64_t prev = offs[level] > 0 ? rd64(um, ind, offs[level] - 1) : 0;

		pref = ffs2_blkpref(ip, lbn, offs[level], prev);

		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb);
		if (error)
			goto fail;

		nb = newb;
		alloc[nalloc].blk = nb;
		alloc[nalloc].ind = ind;
		alloc[nalloc++].idx = offs[level];
		ffs_rw(um, nb, 0, NULL, (size_t)fs->fs_bsize, true, false);
		wr64(um, ind, offs[level], nb);
	}
	*bnp = nb;
	return 0;

fail:
	/*
	 * If we have failed part way through block allocation, we have to
	 * deallocate the blocks that we have allocated, last first, and
	 * the pointers to them.
	 */
	while (nalloc > 0) {
		nalloc--;
		if (alloc[nalloc].ind == 0)
			din->di_ib[alloc[nalloc].idx] = 0;
		else
			wr64(um, alloc[nalloc].ind, alloc[nalloc].idx, 0);
		ffs_blkfree(ip, alloc[nalloc].blk, fs->fs_bsize);
		ip->blocks -= btodb(fs->fs_bsize);
		ip->dirty = true;
	}
	return error;
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * (Adapted from OpenBSD's ffs_indirtrunc(), FFS2 only.)
 */
static int
ffs_indirtrunc(struct inode *ip, daddr_t bn, daddr_t lastbn, int level,
    long *countp)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	int i;
	int64_t *bap, *copy = NULL;
	daddr_t nb, last;
	long blkcount, factor;
	int nblocks, blocksreleased = 0;
	int allerror = 0, error;

	*countp = 0;
	if (!okblk(fs, bn))
		return 0;
	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->fs_bsize);

	/*
	 * Get the block of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.
	 */
	bap = kmalloc((size_t)fs->fs_bsize);
	if (bap == NULL)
		return -ENOMEM;
	ffs_rw(um, bn, 0, bap, (size_t)fs->fs_bsize, false, false);

	if (lastbn != -1) {
		copy = kmalloc((size_t)fs->fs_bsize);
		if (copy == NULL) {
			kfree(bap);
			return -ENOMEM;
		}
		memcpy(copy, bap, (size_t)fs->fs_bsize);

		for (i = (int)last + 1; i < NINDIR(fs); i++)
			bap[i] = 0;
		ffs_rw(um, bn, 0, bap, (size_t)fs->fs_bsize, true, true);
		kfree(bap);
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1; i > last; i--) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = ffs_indirtrunc(ip, nb, -1, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		if (okblk(fs, nb)) {
			ffs_blkfree(ip, nb, fs->fs_bsize);
			blocksreleased += nblocks;
		}
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = bap[i];
		if (nb != 0) {
			error = ffs_indirtrunc(ip, nb, last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}
	kfree(bap);

	*countp = blocksreleased;
	return allerror;
}

/*
 * Truncate the file to length: lengthen it (the last byte is allocated),
 * or shorten it and free the blocks that are no longer wanted.
 *
 * (Adapted from OpenBSD's ffs_truncate(), FFS2 only.)
 */
int
ffs_truncate(struct inode *ip, uint64_t length)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct ufs2_dinode *din = DINO(ip);
	daddr_t lastblock;
	daddr_t bn, lbn, lastiblock[NIADDR], indir_lbn_unused;
	daddr_t oldblks[NDADDR + NIADDR];
	uint64_t osize;
	int offset, size, level, nblocks, i;
	long blocksreleased = 0, count;
	int error, allerror = 0;

	(void)indir_lbn_unused;
	if (ip->sb->rdonly)
		return -EROFS;
	if (S_ISLNK(ip->mode) && ip->size < (uint64_t)fs->fs_maxsymlinklen) {
		memset(din->di_shortlink, 0, (size_t)ip->size);
		ip->size = 0;
		ip->dirty = true;
		return ffs_update(ip, true);
	}

	if (length > um->um_maxfilesize)
		return -EFBIG;

	osize = ip->size;
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		error = ffs_balloc(ip, length - 1, 1, &bn);
		if (error)
			return error;
		ip->size = length;
		pcache_resize(ip);
		ip->dirty = true;
		return ffs_update(ip, true);
	}

	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundary, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever becomes accessible again because
	 * of subsequent file growth. Directories however are not
	 * zero'ed as they should grow back initialized to empty.
	 */
	offset = (int)blkoff(fs, length);
	if (offset == 0) {
		ip->size = length;
	} else {
		lbn = lblkno(fs, length);
		error = ffs_balloc(ip, length - 1, 1, &bn);
		if (error)
			return error;
		ip->size = length;
		size = (int)ffs_blksize(fs, ip->size, lbn);
		if (!S_ISDIR(ip->mode))
			ffs_rw(um, bn, (uint64_t)offset, NULL,
			    (size_t)(size - offset), true, false);
	}
	pcache_resize(ip);

	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - (daddr_t)NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);

	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ffs_indirtrunc below.
	 */
	for (level = TRIPLE; level >= SINGLE; level--) {
		oldblks[NDADDR + level] = din->di_ib[level];
		if (lastiblock[level] < 0) {
			din->di_ib[level] = 0;
			lastiblock[level] = -1;
		}
	}

	for (i = 0; i < NDADDR; i++) {
		oldblks[i] = din->di_db[i];
		if (i > lastblock)
			din->di_db[i] = 0;
	}

	ip->dirty = true;
	if ((error = ffs_update(ip, true)) != 0)
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = oldblks[NDADDR + level];
		if (bn != 0) {
			error = ffs_indirtrunc(ip, bn, lastiblock[level], level,
			    &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				if (okblk(fs, bn)) {
					ffs_blkfree(ip, bn, fs->fs_bsize);
					blocksreleased += nblocks;
				}
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		long bsize;

		bn = oldblks[i];
		if (bn == 0)
			continue;
		bsize = (long)ffs_blksize(fs, osize, i);
		if (okblk(fs, bn)) {
			ffs_blkfree(ip, bn, bsize);
			blocksreleased += btodb(bsize);
		}
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = oldblks[lastblock];
	if (bn != 0 && okblk(fs, bn)) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = (long)ffs_blksize(fs, osize, lastblock);
		newspace = (long)ffs_blksize(fs, length, lastblock);
		if (newspace == 0)
			return -EIO;
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			ffs_blkfree(ip, bn, oldspace - newspace);
			blocksreleased += btodb(oldspace - newspace);
		}
	}
done:
	if (ip->blocks >= (uint64_t)blocksreleased)
		ip->blocks -= (uint64_t)blocksreleased;
	else	/* sanity */
		ip->blocks = 0;
	ip->dirty = true;
	return allerror;
}

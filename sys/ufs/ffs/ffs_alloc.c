/*	$OpenBSD: ffs_alloc.c,v 1.115 2024/02/03 18:51:58 beck Exp $	*/
/*	$NetBSD: ffs_alloc.c,v 1.11 1996/05/11 18:27:09 mycroft Exp $	*/

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
 *	@(#)ffs_alloc.c	8.11 (Berkeley) 10/27/94
 */

/*
 * Adapted for V86BSD from OpenBSD sys/ufs/ffs/ffs_alloc.c (revision 1.115).
 * Changes: the allocation functions (ffs_alloc, ffs_realloccg, ffs_inode_alloc,
 *   ffs_dirpref, ffs2_blkpref, ffs_hashalloc, ffs_fragextend, ffs_alloccg, ffs_alloccgblk,
 *   ffs_nodealloccg, ffs_blkfree, ffs_freefile, ffs_mapsearch) on V86BSD's inode and buffer
 *   cache: FFS2 only, no quota, no cluster maps (a file system that has them is mounted
 *   read-only), cylinder groups are copied out of and back into fragment-sized buffers
 *   (ffs_cgread, ffs_cgwrite), ffs_realloccg moves the contents itself, ffs_inode_alloc
 *   gives the number, not the inode, ffs2_blkpref takes the block before as a value,
 *   a damaged file system gives messages and an allocation that fails, not a panic;
 *   otherwise as OpenBSD.
 */

#include <ufs/ffs/ffs_vfs.h>

static daddr_t	ffs_alloccg(struct inode *, u_int, daddr_t, int);
static daddr_t	ffs_alloccgblk(struct inode *, struct cg *, daddr_t);
static ino_t	ffs_dirpref(struct inode *);
static daddr_t	ffs_fragextend(struct inode *, u_int, daddr_t, int, int);
static daddr_t	ffs_hashalloc(struct inode *, u_int, daddr_t, int,
		    daddr_t (*)(struct inode *, u_int, daddr_t, int));
static daddr_t	ffs_nodealloccg(struct inode *, u_int, daddr_t, int);
static daddr_t	ffs_mapsearch(struct ffsmount *, struct cg *, daddr_t, int);

/* the libkern functions the block maps are searched with */
static int
scanc(u_int size, const u_char *cp, const u_char table[], int mask)
{
	const u_char *end = &cp[size];

	while (cp < end && (table[*cp] & mask) == 0)
		cp++;
	return (int)(end - cp);
}

static int
skpc(int mask, size_t size, const u_char *cp)
{
	const u_char *end = &cp[size];

	while (cp < end && *cp == (u_char)mask)
		cp++;
	return (int)(end - cp);
}

#define isset(a, i)	((a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define isclr(a, i)	(((a)[(i) / NBBY] & (1 << ((i) % NBBY))) == 0)
#define setbit(a, i)	((a)[(i) / NBBY] |= 1 << ((i) % NBBY))
#define clrbit(a, i)	((a)[(i) / NBBY] &= ~(1 << ((i) % NBBY)))

static void
cgtime(struct cg *cgp)
{
	cgp->cg_ffs2_time = (int64_t)(realtime_ns() / 1000000000UL);
	cgp->cg_time = (int32_t)cgp->cg_ffs2_time;
}

/*
 * Allocate a block in the file system.
 *
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadratically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following hierarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadratically rehash into other cylinder groups, until an
 *      available block is located.
 */
int
ffs_alloc(struct inode *ip, daddr_t lbn, daddr_t bpref, int size, daddr_t *bnp)
{
	struct fs *fs = ITOFS(ip);
	daddr_t bno;
	u_int cg;

	(void)lbn;
	*bnp = 0;
	if ((u_int)size > (u_int)fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("ffs: ffs_alloc: bad size %d\n", size);
		return -EIO;
	}
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (cur_proc()->cred.fsuid != 0 && freespace(fs, fs->fs_minfree) <= 0)
		goto nospace;

	/*
	 * Start allocation in the preferred block's cylinder group or
	 * the file's inode's cylinder group if no preferred block was
	 * specified.
	 */
	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = ino_to_cg(fs, (uint32_t)ip->ino);
	else
		cg = dtog(fs, bpref);

	/* Try allocating a block. */
	bno = ffs_hashalloc(ip, cg, bpref, size, ffs_alloccg);
	if (bno > 0) {
		/* allocation successful, update inode data */
		ip->blocks += btodb(size);
		ip->dirty = true;
		*bnp = bno;
		return 0;
	}

nospace:
	ffs_fserr(fs, "file system full");
	return -ENOSPC;
}

/*
 * Reallocate a fragment to a bigger size
 *
 * The number and size of the old block is given, and a preference
 * and new size is also specified. The allocator attempts to extend
 * the original block. Failing that, the regular block allocator is
 * invoked to get an appropriate block.
 *
 * (V86BSD: the data of the file are not in a buffer that is moved along:
 * where the block has to move, the old fragments' contents are copied to
 * the new place, and the added space is zeroed.)
 */
int
ffs_realloccg(struct inode *ip, daddr_t lbprev, daddr_t bpref, int osize,
    int nsize, daddr_t *blknop)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	int request;
	u_int cg;
	daddr_t bprev, bno;

	if ((u_int)osize > (u_int)fs->fs_bsize || fragoff(fs, osize) != 0 ||
	    (u_int)nsize > (u_int)fs->fs_bsize || fragoff(fs, nsize) != 0) {
		printf("ffs: ffs_realloccg: bad size %d, %d\n", osize, nsize);
		return -EIO;
	}
	if (cur_proc()->cred.fsuid != 0 && freespace(fs, fs->fs_minfree) <= 0)
		goto nospace;

	bprev = DINO(ip)->di_db[lbprev];
	if (bprev == 0) {
		printf("ffs: ffs_realloccg: bad bprev, inode %lu\n", (unsigned long)ip->ino);
		ffs_damaged(ITOUM(ip));
		return -EIO;
	}

	/*
	 * Check for extension in the existing location.
	 */
	cg = dtog(fs, bprev);
	if ((bno = ffs_fragextend(ip, cg, bprev, osize, nsize)) != 0) {
		ip->blocks += btodb(nsize - osize);
		ip->dirty = true;
		ffs_rw(um, bno, (uint64_t)osize, NULL, (size_t)(nsize - osize), true, false);
		*blknop = bno;
		return 0;
	}
	/*
	 * Allocate a new disk location.
	 */
	if (bpref >= fs->fs_size)
		bpref = 0;
	switch (fs->fs_optim) {
	case FS_OPTSPACE:
		/*
		 * Allocate an exact sized fragment. Although this makes
		 * best use of space, we will waste time relocating it if
		 * the file continues to grow. If the fragmentation is
		 * less than half of the minimum free reserve, we choose
		 * to begin optimizing for time.
		 */
		request = nsize;
		if (fs->fs_minfree < 5 ||
		    fs->fs_cstotal.cs_nffree >
		    fs->fs_dsize * fs->fs_minfree / (2 * 100))
			break;
		fs->fs_optim = FS_OPTTIME;
		break;
	case FS_OPTTIME:
		/*
		 * At this point we have discovered a file that is trying to
		 * grow a small fragment to a larger fragment. To save time,
		 * we allocate a full sized block, then free the unused portion.
		 * If the file continues to grow, the `ffs_fragextend' call
		 * above will be able to grow it in place without further
		 * copying. If aberrant programs cause disk fragmentation to
		 * grow within 2% of the free reserve, we choose to begin
		 * optimizing for space.
		 */
		request = fs->fs_bsize;
		if (fs->fs_cstotal.cs_nffree <
		    fs->fs_dsize * (fs->fs_minfree - 2) / 100)
			break;
		fs->fs_optim = FS_OPTSPACE;
		break;
	default:
		printf("ffs: ffs_realloccg: bad optim %d\n", fs->fs_optim);
		return -EIO;
	}
	bno = ffs_hashalloc(ip, cg, bpref, request, ffs_alloccg);
	if (bno <= 0)
		goto nospace;

	/* the old contents move to the new place, the rest is zero */
	{
		char *tmp = kmalloc((size_t)nsize);

		if (tmp == NULL) {
			ffs_blkfree(ip, bno, request);
			return -ENOMEM;
		}
		memset(tmp, 0, (size_t)nsize);
		ffs_rw(um, bprev, 0, tmp, (size_t)osize, false, false);
		/*
		 * (V86BSD: the contents go to the disk now, and then the
		 * inode that points to them, before the old fragments are
		 * free to go to another file: if they were reused while the
		 * disk's inode still pointed to them, a crash would leave two
		 * files with the same blocks.)
		 */
		ffs_rw(um, bno, 0, tmp, (size_t)nsize, true, true);
		kfree(tmp);
	}
	DINO(ip)->di_db[lbprev] = bno;
	ffs_update(ip, true);
	ffs_blkfree(ip, bprev, (long)osize);
	if (nsize < request)
		ffs_blkfree(ip, bno + numfrags(fs, nsize),
		    (long)(request - nsize));
	ip->blocks += btodb(nsize - osize);
	ip->dirty = true;
	*blknop = bno;
	return 0;

nospace:
	ffs_fserr(fs, "file system full");
	return -ENOSPC;
}

/*
 * Allocate an inode in the file system.
 *
 * If allocating a directory, use ffs_dirpref to select the inode.
 * If allocating in a directory, the following hierarchy is followed:
 *   1) allocate the preferred inode.
 *   2) allocate an inode in the same cylinder group.
 *   3) quadratically rehash into other cylinder groups, until an
 *      available inode is located.
 * If no inode preference is given the following hierarchy is used
 * to allocate an inode:
 *   1) allocate an inode in cylinder group 0.
 *   2) quadratically rehash into other cylinder groups, until an
 *      available inode is located.
 *
 * (V86BSD: gives the inode's number; the caller makes the inode.)
 */
int
ffs_inode_alloc(struct inode *pip, mode_t mode, ino_t *inop)
{
	struct fs *fs = ITOFS(pip);
	ino_t ino, ipref;
	u_int cg;

	*inop = 0;
	if (fs->fs_cstotal.cs_nifree == 0)
		goto noinodes;

	if ((mode & IFMT) == IFDIR)
		ipref = ffs_dirpref(pip);
	else
		ipref = pip->ino;
	if (ipref >= (ino_t)fs->fs_ncg * fs->fs_ipg)
		ipref = 0;
	cg = ino_to_cg(fs, (uint32_t)ipref);

	/*
	 * Track number of dirs created one after another
	 * in a same cg without intervening by files.
	 */
	if ((mode & IFMT) == IFDIR) {
		if (fs->fs_contigdirs[cg] < 255)
			fs->fs_contigdirs[cg]++;
	} else {
		if (fs->fs_contigdirs[cg] > 0)
			fs->fs_contigdirs[cg]--;
	}
	ino = (ino_t)ffs_hashalloc(pip, cg, (daddr_t)ipref, mode, ffs_nodealloccg);
	if (ino == 0)
		goto noinodes;
	*inop = ino;
	return 0;

noinodes:
	ffs_fserr(fs, "out of inodes");
	return -ENOSPC;
}

/*
 * Find a cylinder group to place a directory.
 *
 * The policy implemented by this algorithm is to allocate a
 * directory inode in the same cylinder group as its parent
 * directory, but also to reserve space for its files inodes
 * and data. Restrict the number of directories which may be
 * allocated one after another in the same cylinder group
 * without intervening allocation of files.
 *
 * If we allocate a first level directory then force allocation
 * in another cylinder group.
 */
static ino_t
ffs_dirpref(struct inode *pip)
{
	struct fs *fs = ITOFS(pip);
	u_int	cg, prefcg;
	u_int	dirsize, cgsize;
	u_int	avgifree, avgbfree, avgndir, curdirsize;
	u_int	minifree, minbfree, maxndir;
	u_int	mincg, minndir;
	u_int	maxcontigdirs;

	avgifree = fs->fs_cstotal.cs_nifree / fs->fs_ncg;
	avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
	avgndir = fs->fs_cstotal.cs_ndir / fs->fs_ncg;

	/*
	 * Force allocation in another cg if creating a first level dir.
	 */
	if (pip->ino == ROOTINO) {
		prefcg = ffs_random(fs->fs_ncg);
		mincg = prefcg;
		minndir = fs->fs_ipg;
		for (cg = prefcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		for (cg = 0; cg < prefcg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		cg = mincg;
		goto end;
	} else
		prefcg = ino_to_cg(fs, (uint32_t)pip->ino);

	/*
	 * Count various limits which used for
	 * optimal allocation of a directory inode.
	 */
	maxndir = MIN(avgndir + fs->fs_ipg / 16, fs->fs_ipg);
	minifree = avgifree - (avgifree / 4);
	if (minifree < 1)
		minifree = 1;
	minbfree = avgbfree - (avgbfree / 4);
	if (minbfree < 1)
		minbfree = 1;

	cgsize = fs->fs_fsize * fs->fs_fpg;
	dirsize = fs->fs_avgfilesize * fs->fs_avgfpdir;
	curdirsize = avgndir ? (cgsize - avgbfree * fs->fs_bsize) / avgndir : 0;
	if (dirsize < curdirsize)
		dirsize = curdirsize;
	if (dirsize <= 0)
		maxcontigdirs = 0;		/* dirsize overflowed */
	else
		maxcontigdirs = MIN(avgbfree * fs->fs_bsize  / dirsize, 255);
	if (fs->fs_avgfpdir > 0)
		maxcontigdirs = MIN(maxcontigdirs,
				    fs->fs_ipg / fs->fs_avgfpdir);
	if (maxcontigdirs == 0)
		maxcontigdirs = 1;

	/*
	 * Limit number of dirs in one cg and reserve space for
	 * regular files, but only if we have no deficit in
	 * inodes or space.
	 *
	 * We are trying to find a suitable cylinder group nearby
	 * our preferred cylinder group to place a new directory.
	 * We scan from our preferred cylinder group forward looking
	 * for a cylinder group that meets our criterion. If we get
	 * to the final cylinder group and do not find anything,
	 * we start scanning forwards from the beginning of the
	 * filesystem. While it might seem sensible to start scanning
	 * backwards or even to alternate looking forward and backward,
	 * this approach fails badly when the filesystem is nearly full.
	 * Specifically, we first search all the areas that have no space
	 * and finally try the one preceding that. We repeat this on
	 * every request and in the case of the final block end up
	 * searching the entire filesystem. By jumping to the front
	 * of the filesystem, our future forward searches always look
	 * in new cylinder groups so finds every possible block after
	 * one pass over the filesystem.
	 */
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
		    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				goto end;
		}
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
		    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				goto end;
		}
	/*
	 * This is a backstop when we have deficit in space.
	 */
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			goto end;
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			goto end;
end:
	return (ino_t)fs->fs_ipg * cg;
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 *
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. The first indirect is allocated immediately following the last
 * direct block and the data blocks for the first indirect immediately
 * follow it.
 *
 * If no blocks have been allocated in any other section, the indirect
 * block(s) are allocated in the same cylinder group as its inode in an
 * area reserved immediately following the inode blocks. The policy for
 * the data blocks is to place them in a cylinder group with a greater than
 * average number of free blocks. An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 *
 * (V86BSD: prev is bap[indx - 1], the block before the one wanted in its
 * pointer array, 0 when there is none; the array itself is not in memory.)
 */
int64_t
ffs2_blkpref(struct inode *ip, daddr_t lbn, int indx, int64_t prev)
{
	struct fs *fs = ITOFS(ip);
	struct ufs2_dinode *din = DINO(ip);
	u_int cg, inocg;
	u_int avgbfree, startcg;
	uint64_t pref;

	/*
	 * Allocation of indirect blocks is indicated by passing negative
	 * values in indx: -1 for single indirect, -2 for double indirect,
	 * -3 for triple indirect. As noted below, we attempt to allocate
	 * the first indirect inline with the file data. For all later
	 * indirect blocks, the data is often allocated in other cylinder
	 * groups. However to speed random file access and to speed up
	 * fsck, the filesystem reserves the first fs_metaspace blocks
	 * (typically half of fs_minfree) of the data area of each cylinder
	 * group to hold these later indirect blocks.
	 */
	inocg = ino_to_cg(fs, (uint32_t)ip->ino);
	if (indx < 0) {
		/*
		 * Our preference for indirect blocks is the zone at the
		 * beginning of the inode's cylinder group data area that
		 * we try to reserve for indirect blocks.
		 */
		pref = cgmeta(fs, inocg);
		/*
		 * If we are allocating the first indirect block, try to
		 * place it immediately following the last direct block.
		 */
		if (indx == -1 && lbn < NDADDR + NINDIR(fs) &&
		    din->di_db[NDADDR - 1] != 0)
			pref = din->di_db[NDADDR - 1] + fs->fs_frag;
		return pref;
	}
	/*
	 * If we are allocating the first data block in the first indirect
	 * block and the indirect has been allocated in the data block area,
	 * try to place it immediately following the indirect block.
	 */
	if (lbn == NDADDR) {
		pref = din->di_ib[0];
		if (pref != 0 && pref >= (uint64_t)cgdata(fs, inocg) &&
		    pref < (uint64_t)cgbase(fs, inocg + 1))
			return pref + fs->fs_frag;
	}
	/*
	 * If we are the beginning of a file, or we have already allocated
	 * the maximum number of blocks per cylinder group, or we do not
	 * have a block allocated immediately preceding us, then we need
	 * to decide where to start allocating new blocks.
	 */

	if (indx % fs->fs_maxbpg == 0 || prev == 0) {
		/*
		 * If we are allocating a directory data block, we want
		 * to place it in the metadata area.
		 */
		if ((ip->mode & IFMT) == IFDIR)
			return cgmeta(fs, inocg);
		/*
		 * Until we fill all the direct and all the first indirect's
		 * blocks, we try to allocate in the data area of the inode's
		 * cylinder group.
		 */
		if (lbn < NDADDR + NINDIR(fs))
			return cgdata(fs, inocg);
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || prev == 0)
			startcg = inocg + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs, prev + 1);

		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;

		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree)
				return cgbase(fs, cg) + fs->fs_frag;

		for (cg = 0; cg < startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree)
				return cgbase(fs, cg) + fs->fs_frag;

		return 0;
	}

	/*
	 * Otherwise, we just always try to lay things out contiguously.
	 */
	return prev + fs->fs_frag;
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadratically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 */
static daddr_t
ffs_hashalloc(struct inode *ip, u_int cg, daddr_t pref, int size,
    daddr_t (*allocator)(struct inode *, u_int, daddr_t, int))
{
	struct fs *fs = ITOFS(ip);
	daddr_t result;
	u_int i, icg = cg;

	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return result;
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return result;
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return result;
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return 0;
}

/*
 * Determine whether a fragment can be extended.
 *
 * Check to see if the necessary fragments are available, and
 * if they are, allocate them.
 */
static daddr_t
ffs_fragextend(struct inode *ip, u_int cg, daddr_t bprev, int osize, int nsize)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct cg *cgp;
	daddr_t bno;
	int i, frags, bbase;

	if (fs->fs_cs(fs, cg).cs_nffree < numfrags(fs, nsize - osize))
		return 0;
	frags = numfrags(fs, nsize);
	bbase = fragnum(fs, bprev);
	if (bbase > fragnum(fs, (bprev + frags - 1))) {
		/* cannot extend across a block boundary */
		return 0;
	}

	if ((cgp = ffs_cgread(um, cg)) == NULL)
		return 0;

	cgtime(cgp);

	bno = dtogd(fs, bprev);
	for (i = numfrags(fs, osize); i < frags; i++)
		if (isclr(cg_blksfree(cgp), bno + i)) {
			kfree(cgp);
			return 0;
		}
	/*
	 * the current fragment can be extended
	 * deduct the count on fragment being extended into
	 * increase the count on the remaining fragment (if any)
	 * allocate the extended piece
	 */
	for (i = frags; i < fs->fs_frag - bbase; i++)
		if (isclr(cg_blksfree(cgp), bno + i))
			break;
	cgp->cg_frsum[i - numfrags(fs, osize)]--;
	if (i != frags)
		cgp->cg_frsum[i - frags]++;
	for (i = numfrags(fs, osize); i < frags; i++) {
		clrbit(cg_blksfree(cgp), bno + i);
		cgp->cg_cs.cs_nffree--;
		fs->fs_cstotal.cs_nffree--;
		fs->fs_cs(fs, cg).cs_nffree--;
	}
	fs->fs_fmod = 1;

	ffs_cgwrite(um, cg, cgp);
	return bprev;
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static daddr_t
ffs_alloccg(struct inode *ip, u_int cg, daddr_t bpref, int size)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct cg *cgp;
	daddr_t bno, blkno;
	int i, frags, allocsiz;

	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return 0;

	if ((cgp = ffs_cgread(um, cg)) == NULL)
		return 0;

	if (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize) {
		kfree(cgp);
		return 0;
	}

	cgtime(cgp);

	if (size == fs->fs_bsize) {
		/* allocate and return a complete data block */
		bno = ffs_alloccgblk(ip, cgp, bpref);
		ffs_cgwrite(um, cg, cgp);
		return bno;
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0) {
			kfree(cgp);
			return 0;
		}
		bno = ffs_alloccgblk(ip, cgp, bpref);
		if (bno == 0) {
			kfree(cgp);
			return 0;
		}
		bpref = dtogd(fs, bno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(cg_blksfree(cgp), bpref + i);
		i = fs->fs_frag - frags;
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		cgp->cg_frsum[i]++;
		ffs_cgwrite(um, cg, cgp);
		return bno;
	}
	bno = ffs_mapsearch(ITOUM(ip), cgp, bpref, allocsiz);
	if (bno < 0) {
		kfree(cgp);
		return 0;
	}

	for (i = 0; i < frags; i++)
		clrbit(cg_blksfree(cgp), bno + i);
	cgp->cg_cs.cs_nffree -= frags;
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	cgp->cg_frsum[allocsiz]--;
	if (frags != allocsiz)
		cgp->cg_frsum[allocsiz - frags]++;

	blkno = cgbase(fs, cg) + bno;
	ffs_cgwrite(um, cg, cgp);
	return blkno;
}

/*
 * Allocate a block in a cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
static daddr_t
ffs_alloccgblk(struct inode *ip, struct cg *cgp, daddr_t bpref)
{
	struct fs *fs = ITOFS(ip);
	daddr_t bno, blkno;
	u_int8_t *blksfree;
	int cgbpref;

	blksfree = cg_blksfree(cgp);

	if (bpref == 0) {
		bpref = cgp->cg_rotor;
	} else if ((cgbpref = dtog(fs, bpref)) != (int)cgp->cg_cgx) {
		/* map bpref to correct zone in this cg */
		if (bpref < cgdata(fs, cgbpref))
			bpref = cgmeta(fs, cgp->cg_cgx);
		else
			bpref = cgdata(fs, cgp->cg_cgx);
	}
	/*
	 * If the requested block is available, use it.
	 */
	bno = dtogd(fs, blknum(fs, bpref));
	if (ffs_isblock(fs, blksfree, fragstoblks(fs, bno)))
		goto gotit;
	/*
	 * Take the next available block in this cylinder group.
	 */
	bno = ffs_mapsearch(ITOUM(ip), cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return 0;

	/* Update cg_rotor only if allocated from the data zone */
	if (bno >= dtogd(fs, cgdata(fs, cgp->cg_cgx)))
		cgp->cg_rotor = (uint32_t)bno;

gotit:
	blkno = fragstoblks(fs, bno);
	ffs_clrblock(fs, blksfree, blkno);
	/* (no cluster maps: a file system with them is read-only) */
	cgp->cg_cs.cs_nbfree--;
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, cgp->cg_cgx).cs_nbfree--;

	fs->fs_fmod = 1;
	blkno = cgbase(fs, cgp->cg_cgx) + bno;

	return blkno;
}

/* inode allocation routine */
static daddr_t
ffs_nodealloccg(struct inode *ip, u_int cg, daddr_t ipref, int mode)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct cg *cgp;
	int start, len, loc, map, i;
	bool advanced = false;

	/*
	 * For efficiency, before looking at the bitmaps for free inodes,
	 * check the counters kept in the superblock cylinder group summaries,
	 * and in the cylinder group itself.
	 */
	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		return 0;

	if ((cgp = ffs_cgread(um, cg)) == NULL)
		return 0;

	if (cgp->cg_cs.cs_nifree == 0) {
		kfree(cgp);
		return 0;
	}

	/*
	 * We are committed to the allocation from now on, so update the time
	 * on the cylinder group.
	 */
	cgtime(cgp);

	/*
	 * If there was a preferred location for the new inode, try to find it.
	 */
	if (ipref) {
		ipref %= fs->fs_ipg;
		if (isclr(cg_inosused(cgp), ipref))
			goto gotit; /* inode is free, grab it. */
	}

	/*
	 * Otherwise, look for the next available inode, starting at cg_irotor
	 * (the position in the bitmap of the last used inode).
	 */
	start = cgp->cg_irotor / NBBY;
	len = howmany(fs->fs_ipg - cgp->cg_irotor, NBBY);
	loc = skpc(0xff, len, &cg_inosused(cgp)[start]);
	if (loc == 0) {
		/*
		 * If we didn't find a free inode in the upper part of the
		 * bitmap (from cg_irotor to the end), then look at the bottom
		 * part (from 0 to cg_irotor).
		 */
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &cg_inosused(cgp)[0]);
		if (loc == 0) {
			/*
			 * If we failed again, then either the bitmap or the
			 * counters kept for the cylinder group are wrong.
			 */
			printf("ffs: cg = %u, irotor = %u: inode map corrupted, run fsck_ffs\n",
			    cg, cgp->cg_irotor);
			ffs_damaged(ITOUM(ip));
			kfree(cgp);
			return 0;
		}
	}

	/* skpc() returns the position relative to the end */
	i = start + len - loc;

	/*
	 * Okay, so now in 'i' we have the location in the bitmap of a byte
	 * holding a free inode. Find the corresponding bit and set it,
	 * updating cg_irotor as well, accordingly.
	 */
	map = cg_inosused(cgp)[i];
	ipref = i * NBBY;
	for (i = 1; i < (1 << NBBY); i <<= 1, ipref++) {
		if ((map & i) == 0) {
			cgp->cg_irotor = (uint32_t)ipref;
			goto gotit;
		}
	}

	printf("ffs: cg = %u: block not in inode map, run fsck_ffs\n", cg);
	ffs_damaged(ITOUM(ip));
	kfree(cgp);
	return 0;

gotit:
	/*
	 * For FFS2, check if all inodes in this cylinder group have been used
	 * at least once. If they haven't, and we are allocating an inode past
	 * the last allocated block of inodes, read in a block and initialize
	 * all inodes in it.
	 */
	if (/* Inode is beyond last initialized block of inodes? */
	    ipref + INOPB(fs) > cgp->cg_initediblk &&
	    /* Has any inode not been used at least once? */
	    cgp->cg_initediblk < cgp->cg_ffs2_niblk) {
		struct ufs2_dinode *dp2 = kzalloc(fs->fs_bsize);

		if (dp2 != NULL) {
			/* Give each inode a generation number */
			for (i = 0; i < (int)INOPB(fs); i++) {
				while (dp2[i].di_gen == 0)
					dp2[i].di_gen = (int32_t)ffs_random(0xffffffffU);
			}
			/*
			 * (V86BSD: the block of inodes and the group that says it
			 * is in use go to the disk now, in that order: fsck
			 * takes the inodes beyond cg_initediblk for unused, so
			 * after a crash the inode written next must be inside
			 * it, and what is inside must have been initialized.)
			 */
			ffs_rw(um, ino_to_fsba(fs, cg * fs->fs_ipg + cgp->cg_initediblk),
			    0, dp2, fs->fs_bsize, true, true);
			kfree(dp2);

			/* Update the counter of initialized inodes */
			cgp->cg_initediblk += INOPB(fs);
			advanced = true;
		}
	}

	setbit(cg_inosused(cgp), ipref);

	/* Update the counters we keep on free inodes */
	cgp->cg_cs.cs_nifree--;
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	fs->fs_fmod = 1; /* file system was modified */

	/* Update the counters we keep on allocated directories */
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir++;
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}

	if (advanced) {
		ffs_rw(um, cgtod(fs, cg), 0, cgp, fs->fs_cgsize, true, true);
		kfree(cgp);
	} else
		ffs_cgwrite(um, cg, cgp);

	/* Return the allocated inode number */
	return cg * fs->fs_ipg + ipref;
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible
 * block reassembly is checked.
 */
void
ffs_blkfree(struct inode *ip, daddr_t bno, long size)
{
	struct ffsmount *um = ITOUM(ip);
	struct fs *fs = um->um_fs;
	struct cg *cgp;
	daddr_t blkno;
	int i, cg, blk, frags, bbase;

	if (um->um_damaged)	/* (what is left to free is for fsck_ffs) */
		return;

	if ((u_int)size > (u_int)fs->fs_bsize || fragoff(fs, size) != 0 ||
	    fragnum(fs, bno) + numfrags(fs, size) > fs->fs_frag) {
		printf("ffs: ffs_blkfree: bad size %ld, inode %lu\n", size,
		    (unsigned long)ip->ino);
		return;
	}
	cg = dtog(fs, bno);
	if (bno < 0 || bno >= fs->fs_size) {
		printf("ffs: bad block %lld, inode %lu\n", (long long)bno,
		    (unsigned long)ip->ino);
		ffs_fserr(fs, "bad block");
		ffs_damaged(um);
		return;
	}
	if ((cgp = ffs_cgread(um, cg)) == NULL)
		return;

	cgtime(cgp);

	bno = dtogd(fs, bno);
	if (size == fs->fs_bsize) {
		blkno = fragstoblks(fs, bno);
		if (!ffs_isfreeblock(fs, cg_blksfree(cgp), blkno)) {
			printf("ffs: block %lld freed, but is free, run fsck_ffs\n",
			    (long long)bno);
			ffs_damaged(um);
			kfree(cgp);
			return;
		}
		ffs_setblock(fs, cg_blksfree(cgp), blkno);
		cgp->cg_cs.cs_nbfree++;
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
	} else {
		bbase = bno - fragnum(fs, bno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp), bbase);
		ffs_fragacct(fs, blk, (int32_t *)cgp->cg_frsum, -1);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(cg_blksfree(cgp), bno + i)) {
				printf("ffs: fragment %lld freed, but is free, run fsck_ffs\n",
				    (long long)(bno + i));
				ffs_damaged(um);
				blk = blkmap(fs, cg_blksfree(cgp), bbase);
				ffs_fragacct(fs, blk, (int32_t *)cgp->cg_frsum, 1);
				kfree(cgp);
				return;
			}
			setbit(cg_blksfree(cgp), bno + i);
		}
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp), bbase);
		ffs_fragacct(fs, blk, (int32_t *)cgp->cg_frsum, 1);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		blkno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, cg_blksfree(cgp), blkno)) {
			cgp->cg_cs.cs_nffree -= fs->fs_frag;
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			cgp->cg_cs.cs_nbfree++;
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
		}
	}
	fs->fs_fmod = 1;
	ffs_cgwrite(um, cg, cgp);
}

/*
 * Do the actual free operation.
 * The specified inode is placed back in the free map.
 */
void
ffs_freefile(struct inode *pip, ino_t ino, mode_t mode)
{
	struct ffsmount *um = ITOUM(pip);
	struct fs *fs = um->um_fs;
	struct cg *cgp;
	u_int cg;

	if (um->um_damaged)
		return;
	if (ino >= (ino_t)fs->fs_ipg * fs->fs_ncg) {
		printf("ffs: ffs_freefile: inode %lu out of range\n", (unsigned long)ino);
		ffs_damaged(um);
		return;
	}

	cg = ino_to_cg(fs, (uint32_t)ino);
	if ((cgp = ffs_cgread(um, cg)) == NULL)
		return;

	cgtime(cgp);

	ino %= fs->fs_ipg;
	if (isclr(cg_inosused(cgp), ino)) {
		printf("ffs: inode %lu freed, but is free, run fsck_ffs\n",
		    (unsigned long)ino);
		ffs_damaged(um);
		kfree(cgp);
		return;
	}
	clrbit(cg_inosused(cgp), ino);
	if (ino < cgp->cg_irotor)
		cgp->cg_irotor = (uint32_t)ino;
	cgp->cg_cs.cs_nifree++;
	fs->fs_cstotal.cs_nifree++;
	fs->fs_cs(fs, cg).cs_nifree++;
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir--;
		fs->fs_cstotal.cs_ndir--;
		fs->fs_cs(fs, cg).cs_ndir--;
	}
	fs->fs_fmod = 1;
	ffs_cgwrite(um, cg, cgp);
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * (OpenBSD panics when a request is made to find a block and none is
 * available, or the map is wrong; here the message is the same and the
 * request fails with -1.)
 */
static daddr_t
ffs_mapsearch(struct ffsmount *um, struct cg *cgp, daddr_t bpref, int allocsiz)
{
	struct fs *fs = um->um_fs;
	daddr_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = cgp->cg_frotor / NBBY;
	len = howmany(fs->fs_fpg, NBBY) - start;
	loc = scanc((u_int)len, (u_char *)&cg_blksfree(cgp)[start],
		(u_char *)fragtbl[fs->fs_frag],
		(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len, (u_char *)&cg_blksfree(cgp)[0],
			(u_char *)fragtbl[fs->fs_frag],
			(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			printf("ffs: cg %u: map corrupted, run fsck_ffs\n", cgp->cg_cgx);
			ffs_damaged(um);
			return -1;
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = (uint32_t)bno;
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, cg_blksfree(cgp), bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return bno + pos;
			field <<= 1;
			subfield <<= 1;
		}
	}
	printf("ffs: cg %u: block not in map, run fsck_ffs\n", cgp->cg_cgx);
	ffs_damaged(um);
	return -1;
}

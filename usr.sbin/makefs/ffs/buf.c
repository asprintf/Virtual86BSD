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
 * makefs: block I/O on the image for the file system code from NetBSD.
 * A buffer is memory for one block: bread() fills it from the image,
 * bwrite() writes it and frees it, brelse() frees it.  There is no cache,
 * so what a block holds is always what the image holds.
 *
 * The one thing that is kept is the number of the sector of an indirect
 * block: ffs_balloc() gives those blocks negative logical block numbers
 * and finds them again by that number, so a table maps a negative number
 * to the sector it was written to.
 */

#include <assert.h>
#include <unistd.h>

#include "makefs.h"
#include "ffs/buf.h"

struct map {
	daddr_t		 lblkno, blkno;
	struct map	*next;
};

static struct map *maps;

static daddr_t
lookup(daddr_t lblkno)
{
	struct map *m;

	for (m = maps; m != NULL; m = m->next)
		if (m->lblkno == lblkno)
			return m->blkno;
	return lblkno;
}

static void
remember(daddr_t lblkno, daddr_t blkno)
{
	struct map *m;

	for (m = maps; m != NULL; m = m->next)
		if (m->lblkno == lblkno) {
			m->blkno = blkno;
			return;
		}
	m = ecalloc(1, sizeof(*m));
	m->lblkno = lblkno;
	m->blkno = blkno;
	m->next = maps;
	maps = m;
}

struct buf *
getblk(struct vnode *vp, daddr_t blkno, int size, int u1 __unused, int u2 __unused)
{
	struct buf *bp = ecalloc(1, sizeof(*bp));

	bp->b_lblkno = blkno;
	bp->b_blkno = blkno < 0 ? lookup(blkno) : blkno;
	bp->b_fs = vp->fs;
	bp->b_bcount = bp->b_bufsize = size;
	bp->b_data = ecalloc(1, (size_t)size);
	return bp;
}

int
bread(struct vnode *vp, daddr_t blkno, int size, int u2 __unused, struct buf **bpp)
{
	struct buf *bp = getblk(vp, blkno, size, 0, 0);
	off_t offset = (off_t)bp->b_blkno * bp->b_fs->sectorsize + bp->b_fs->offset;
	ssize_t n;

	n = pread(bp->b_fs->fd, bp->b_data, (size_t)size, offset);
	if (n != size) {
		if (n == -1)
			warn("reading block %jd of the image", (intmax_t)bp->b_blkno);
		else
			warnx("block %jd of the image is beyond its end", (intmax_t)bp->b_blkno);
		exit(EXIT_FAILURE);
	}
	*bpp = bp;
	return 0;
}

int
bwrite(struct buf *bp)
{
	off_t offset = (off_t)bp->b_blkno * bp->b_fs->sectorsize + bp->b_fs->offset;
	ssize_t n;
	int e = 0;

	n = pwrite(bp->b_fs->fd, bp->b_data, (size_t)bp->b_bcount, offset);
	if (n != bp->b_bcount)
		e = n == -1 ? errno : EAGAIN;
	brelse(bp, 0);
	return e;
}

void
brelse(struct buf *bp, int u1 __unused)
{
	assert(bp != NULL);
	if (bp->b_lblkno < 0)
		remember(bp->b_lblkno, bp->b_blkno);
	free(bp->b_data);
	free(bp);
}

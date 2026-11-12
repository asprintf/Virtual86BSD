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
 * makefs: the buffers of the file system code taken from NetBSD (ffs_alloc.c,
 * ffs_balloc.c, ufs_bmap.c): each is a block of the image, read and written
 * at once (there is no cache), and what the kernel's names mean there.  The
 * names are those of the kernel interface that code is written to.
 */

#ifndef _FFS_BUF_H
#define _FFS_BUF_H

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "nbtool_config.h"
#include "namespace.h"	/* daddr_t */

struct vnode {
	struct fsinfo	*fs;		/* the image the blocks are in */
};

struct buf {
	void		*b_data;
	long		 b_bufsize;
	long		 b_bcount;
	daddr_t		 b_blkno;	/* where in the image, in sectors */
	daddr_t		 b_lblkno;	/* as asked for; a negative number names an indirect block */
	struct fsinfo	*b_fs;
};

int		 bread(struct vnode *, daddr_t, int, int, struct buf **);
void		 brelse(struct buf *, int);
int		 bwrite(struct buf *);
struct buf	*getblk(struct vnode *, daddr_t, int, int, int);

#define	bdwrite(bp)	bwrite(bp)
#define	clrbuf(bp)	memset((bp)->b_data, 0, (size_t)(bp)->b_bcount)

#define	B_MODIFY	0
#define	BC_AGE		0
#define	NOCRED		NULL
#define	IO_SYNC		1
#define	DEV_BSHIFT	9
#define	KASSERT(a)

#define	min(a, b)	MIN((a), (b))

static inline unsigned int
uimin(unsigned int a, unsigned int b)
{
	return a < b ? a : b;
}

static inline unsigned int
uimax(unsigned int a, unsigned int b)
{
	return a > b ? a : b;
}

#endif

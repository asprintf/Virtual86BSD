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
/* makefs: what ffs_mkfs() takes of the options for the file system (FFS2) */

#ifndef _MAKEFS_FFS_H
#define _MAKEFS_FFS_H

typedef struct {
	int	bsize;		/* block size */
	int	fsize;		/* fragment size */
	int	cpg;		/* (not used: the geometry is made up) */
	int	density;	/* bytes per inode */
	int	minfree;	/* percent kept free */
	int	optimization;	/* FS_OPTTIME or FS_OPTSPACE */
	int	maxcontig;
	int	maxbpg;		/* blocks of one file in a group */
	int	avgfilesize;
	int	avgfpdir;	/* files per directory */
	int	maxbsize;	/* the largest extent */
	int	maxblkspercg;
	int	version;	/* 2: only FFS2 is made */
	int	extattr;	/* 0: the plain FFS2 magic number */
	char	label[16];
} ffs_opt_t;

#endif

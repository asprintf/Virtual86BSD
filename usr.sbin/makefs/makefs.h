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
 * makefs: the types the tree walker, the image builder and the file system
 * code share.  Written for V86BSD; makefs builds an FFS image from a
 * directory tree, as NetBSD's makefs(8) does (whose FFS allocator it uses,
 * see README.md).
 */

#ifndef _MAKEFS_H
#define _MAKEFS_H

#include "nbtool_config.h"

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

/* what the tree holds for one file: its names (hard links) share it */
typedef struct fsinode {
	struct stat	 st;		/* from the tree, uid and gid maybe root's */
	uint32_t	 nlink;		/* how many names the tree has for it */
	uint32_t	 ino;		/* its number in the image (0: none yet) */
	bool		 written;	/* its inode is in the image */
} fsinode;

/* one name in the tree */
typedef struct fsnode {
	struct fsnode	*next;		/* the next name in the same directory */
	struct fsnode	*child;		/* a directory: its first entry */
	struct fsnode	*parent;	/* the directory it is in (NULL: the top) */
	fsinode		*inode;
	char		*name;
	char		*path;		/* its path in the tree (to read it) */
	char		*symlink;	/* a symbolic link: where it points */
	uint32_t	 type;		/* st_mode & S_IFMT */
} fsnode;

/* what is wanted of the image, and what is known of it while it is made */
typedef struct fsinfo {
	int		 fd;		/* the image file */
	off_t		 size;		/* of the file system, in bytes */
	off_t		 offset;	/* where in the image file it starts */
	int		 sectorsize;
	int		 needswap;	/* always 0: the image has the byte order of this host */
	off_t		 minsize, maxsize;
	off_t		 freeblocks;	/* bytes to leave free, more than the tree needs */
	int		 freeblockpc;	/* percent more */
	off_t		 freefiles;	/* inodes to leave free */
	int		 freefilepc;
	off_t		 inodes;	/* how many the file system needs */
	uint32_t	 curinode;	/* the next inode number to give */
	void		*superblock;	/* struct fs, from ffs_mkfs() */
	void		*fs_specific;	/* ffs_opt_t */
} fsinfo_t;

/* makefs.c */
extern bool	 rootowner;		/* -U: all files are root's */
extern bool	 verbose;		/* -v */
extern bool	 stamped;		/* -T: all times are stamp */
extern time_t	 stamp;
void		 panic(const char *, ...)
    __attribute__((__noreturn__, __format__(__printf__, 1, 2)));

/* walk.c */
fsnode		*walk_dir(const char *);
void		 free_fsnodes(fsnode *);

/* image.c */
void		 ffs_prep_opts(fsinfo_t *);
int		 ffs_parse_opts(const char *, fsinfo_t *);
void		 ffs_makefs(const char *, fsnode *, fsinfo_t *);

#endif

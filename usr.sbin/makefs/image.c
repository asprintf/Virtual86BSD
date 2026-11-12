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
 * makefs: make the FFS2 image: work out how big it has to be, make the file
 * system with ffs_mkfs() (mkfs.c, from NetBSD), and write the tree into it
 * with the allocator of NetBSD's kernel (ffs_balloc() and ffs_alloc() on the
 * buffers of buf.c).  Written for V86BSD, with FFS2 as the only format.
 */

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "makefs.h"
#include "ffs.h"

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ffs/fs.h>

#include "ffs/buf.h"
#include "ffs/ufs_inode.h"
#include "ffs/newfs_extern.h"
#include "ffs/ffs_extern.h"

#define	DFL_BSIZE	16384		/* as newfs makes them */
#define	DFL_FSIZE	2048
#define	DFL_SECTOR	512

static ffs_opt_t *
opts_of(const fsinfo_t *f)
{
	return f->fs_specific;
}

/* ---- the options (-o) ---- */

void
ffs_prep_opts(fsinfo_t *f)
{
	ffs_opt_t *o = ecalloc(1, sizeof(*o));

	o->bsize = DFL_BSIZE;
	o->fsize = DFL_FSIZE;
	o->density = -1;			/* from the size and the inodes */
	o->minfree = MINFREE;
	o->optimization = DEFAULTOPT;
	o->maxcontig = 1;			/* the driver keeps no cluster maps */
	o->maxbpg = -1;
	o->avgfilesize = AVFILESIZ;
	o->avgfpdir = AFPDIR;
	o->maxbsize = -1;
	o->version = 2;
	f->fs_specific = o;
	f->sectorsize = DFL_SECTOR;
}

/* one -o option: name or name=value */
int
ffs_parse_opts(const char *option, fsinfo_t *f)
{
	ffs_opt_t *o = opts_of(f);
	char *var = estrdup(option), *val;
	int ok = 1;
	if ((val = strchr(var, '=')) != NULL)
		*val++ = '\0';
	else
		val = "";
#define	NUM(field, min, max) \
	(o->field = (int)strsuftoll(var, val, min, max), 1)
	if (!strcmp(var, "bsize"))
		NUM(bsize, 1, INT_MAX);
	else if (!strcmp(var, "fsize"))
		NUM(fsize, 1, INT_MAX);
	else if (!strcmp(var, "density"))
		NUM(density, 1, INT_MAX);
	else if (!strcmp(var, "minfree"))
		NUM(minfree, 0, 99);
	else if (!strcmp(var, "maxbpg"))
		NUM(maxbpg, 1, INT_MAX);
	else if (!strcmp(var, "avgfilesize"))
		NUM(avgfilesize, 1, INT_MAX);
	else if (!strcmp(var, "avgfpdir"))
		NUM(avgfpdir, 1, INT_MAX);
	else if (!strcmp(var, "extent"))
		NUM(maxbsize, 1, INT_MAX);
	else if (!strcmp(var, "maxbpcg"))
		NUM(maxblkspercg, 1, INT_MAX);
	else if (!strcmp(var, "optimization")) {
		if (!strcmp(val, "time"))
			o->optimization = FS_OPTTIME;
		else if (!strcmp(val, "space"))
			o->optimization = FS_OPTSPACE;
		else {
			warnx("Invalid optimization `%s' (time or space)", val);
			ok = 0;
		}
	} else if (!strcmp(var, "label")) {
		if (strlen(val) >= sizeof(o->label)) {
			warnx("The label `%s' is too long (%zu bytes at most)", val, sizeof(o->label) - 1);
			ok = 0;
		} else
			strlcpy(o->label, val, sizeof(o->label));
	} else {
		warnx("Unknown option `%s'", var);
		ok = 0;
	}
	free(var);
	return ok;
}

void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

/* ---- directories ---- */

static uint8_t
dtype(uint32_t type)
{
	return (uint8_t)IFTODT(type);
}

/* the space an entry of that name takes: the 8 bytes of the head and the name with its NUL, to 4 */
static size_t
entsize(const char *name)
{
	return 8 + roundup(strlen(name) + 1, 4);
}

/*
 * The directory as the file system has it: chunks of UFS_DIRBLKSIZ bytes in
 * which the entries follow each other, the last of a chunk reaching its end.
 * Entry 0 is ".", 1 is "..", then those of the tree.  The bytes are made
 * (and *lenp is their number) if buf is not NULL, else only counted.
 */
static size_t
dir_pack(const fsnode *dir, uint32_t ino, uint32_t pino, uint8_t *buf)
{
	const fsnode *c;
	size_t len = 0, chunk_start = 0, last = 0, i;
	struct {
		const char *name;
		uint32_t ino;
		uint8_t type;
	} e;
	int step = 0;

	for (c = dir->child; ; ) {
		size_t sz;

		if (step == 0) {
			e.name = "."; e.ino = ino; e.type = DT_DIR;
		} else if (step == 1) {
			e.name = ".."; e.ino = pino; e.type = DT_DIR;
		} else {
			if (c == NULL)
				break;
			e.name = c->name; e.ino = c->inode->ino; e.type = dtype(c->type);
		}
		sz = entsize(e.name);
		if (len > chunk_start && len + sz > chunk_start + UFS_DIRBLKSIZ) {
			/* it does not fit in this chunk: the last entry of it reaches the end */
			if (buf != NULL) {
				struct direct *lp = (struct direct *)(buf + last);

				lp->d_reclen = (uint16_t)(chunk_start + UFS_DIRBLKSIZ - last);
			}
			len = chunk_start += UFS_DIRBLKSIZ;
		}
		if (buf != NULL) {
			struct direct *dp = (struct direct *)(buf + len);

			memset(dp, 0, sz);
			dp->d_fileno = e.ino;
			dp->d_reclen = (uint16_t)sz;
			dp->d_type = e.type;
			dp->d_namlen = (uint8_t)strlen(e.name);
			memcpy(dp->d_name, e.name, strlen(e.name));
		}
		last = len;
		len += sz;
		if (step < 2)
			step++;
		else
			c = c->next;
	}
	/* the last chunk: its last entry reaches the end */
	len = chunk_start + UFS_DIRBLKSIZ;
	if (buf != NULL) {
		struct direct *lp = (struct direct *)(buf + last);

		lp->d_reclen = (uint16_t)(len - last);
	}
	(void)i;
	return len;
}

/* ---- the size ---- */

struct tally {
	uint64_t bytes;
	uint64_t inodes;
};

static uint64_t
file_bytes(uint64_t size, const ffs_opt_t *o)
{
	uint64_t nblk = (size + o->bsize - 1) / o->bsize, bytes, nind = o->bsize / sizeof(int64_t);

	bytes = roundup(size, (uint64_t)o->fsize);
	if (nblk > UFS_NDADDR) {
		uint64_t rest = nblk - UFS_NDADDR, ind = (rest + nind - 1) / nind;

		bytes += ind * o->bsize;			/* the indirect blocks */
		if (rest > nind)
			bytes += (1 + (rest - nind + nind * nind - 1) / (nind * nind)) * o->bsize;
	}
	return bytes;
}

static void
tally_dir(const fsnode *dir, const ffs_opt_t *o, struct tally *t)
{
	const fsnode *c;

	t->bytes += roundup(dir_pack(dir, 0, 0, NULL), (size_t)o->fsize);
	for (c = dir->child; c != NULL; c = c->next) {
		if (c->type == S_IFDIR) {
			t->inodes++;
			tally_dir(c, o, t);
			continue;
		}
		if (c->inode->ino == (uint32_t)-1)
			continue;			/* another name of a file counted */
		c->inode->ino = (uint32_t)-1;
		t->inodes++;
		if (c->type == S_IFREG)
			t->bytes += file_bytes((uint64_t)c->inode->st.st_size, o);
		else if (c->type == S_IFLNK && strlen(c->symlink) >= UFS2_MAXSYMLINKLEN)
			t->bytes += roundup(strlen(c->symlink), (size_t)o->fsize);
	}
}

static void
clear_marks(fsnode *dir)
{
	fsnode *c;

	for (c = dir->child; c != NULL; c = c->next) {
		if (c->type != S_IFDIR)
			c->inode->ino = 0;
		else
			clear_marks(c);
	}
}

/* the size and the number of inodes the image needs, as opts and -b -f -M say */
static void
validate(fsnode *root, fsinfo_t *f)
{
	ffs_opt_t *o = opts_of(f);
	struct tally t = {0, 1};		/* the root */
	uint64_t size, ncg, density;

	if (o->fsize < f->sectorsize)
		o->fsize = f->sectorsize;
	if (o->bsize < o->fsize)
		o->bsize = o->fsize;
	if (o->maxbpg == -1)
		o->maxbpg = o->bsize / (int)sizeof(int64_t);
	if (o->maxbsize == -1)
		o->maxbsize = o->bsize;
	tally_dir(root, o, &t);
	clear_marks(root);
	t.inodes += UFS_ROOTINO;		/* inodes 0 and 1 are not used, 2 is the root */
	if (verbose)
		printf("makefs: the tree needs %ju bytes and %ju inodes\n", (uintmax_t)t.bytes, (uintmax_t)t.inodes);

	t.bytes += f->freeblocks;
	t.inodes += f->freefiles;
	if (f->freefilepc > 0)
		t.inodes = t.inodes * (100 + f->freefilepc) / 100;
	if (f->freeblockpc > 0)
		t.bytes = t.bytes * (100 + f->freeblockpc) / 100;
	t.bytes += t.inodes * DINODE2_SIZE;
	t.bytes += t.bytes / 20;			/* partly filled fragments, maps */
	if (o->minfree > 0)
		t.bytes = t.bytes * (100 + o->minfree) / 100;
	ncg = t.bytes / (32 << 20) + 1;
	t.bytes += ncg * (SBLOCK_UFS2 + SBLOCKSIZE + 4 * (uint64_t)o->bsize);
	size = t.bytes;
	/* the file system makes an inode for each density bytes, and that is not less than a fragment */
	if (o->density == -1 && size < (t.inodes + t.inodes / 4 + 16) * (uint64_t)o->fsize)
		size = (t.inodes + t.inodes / 4 + 16) * (uint64_t)o->fsize;
	if (size < (uint64_t)f->minsize)
		size = (uint64_t)f->minsize;
	size = roundup(size, (uint64_t)o->bsize);
	if (f->maxsize > 0 && size > (uint64_t)f->maxsize)
		errx(EXIT_FAILURE, "the image has to be %ju bytes, more than the maximum of %jd",
		    (uintmax_t)size, (intmax_t)f->maxsize);
	f->size = (off_t)size;
	f->inodes = (off_t)t.inodes;
	if (o->density == -1) {
		density = size / (t.inodes + t.inodes / 4 + 16);
		o->density = (int)MAX(density, (uint64_t)o->fsize);
	}
}

/* ---- the inodes ---- */

static void
set_times(struct ufs2_dinode *dp, const struct stat *st)
{
	dp->di_atime = st->st_atim.tv_sec;
	dp->di_atimensec = (uint32_t)st->st_atim.tv_nsec;
	dp->di_mtime = st->st_mtim.tv_sec;
	dp->di_mtimensec = (uint32_t)st->st_mtim.tv_nsec;
	dp->di_ctime = st->st_ctim.tv_sec;
	dp->di_ctimensec = (uint32_t)st->st_ctim.tv_nsec;
	dp->di_birthtime = st->st_mtim.tv_sec;
	dp->di_birthnsec = (uint32_t)st->st_mtim.tv_nsec;
}

/* the inode in the file system's tables (the cylinder group's map and counts) and on its disk block */
static void
write_inode(const struct ufs2_dinode *dp, uint32_t ino, fsinfo_t *f)
{
	struct fs *fs = f->superblock;
	struct cg *cgp;
	uint32_t cg = ino_to_cg(fs, ino), cgino = ino % fs->fs_ipg, initediblk, i;
	char *cgbuf = emalloc((size_t)fs->fs_cgsize), *blk = emalloc((size_t)fs->fs_bsize);
	struct ufs2_dinode *dip;
	daddr_t d;

	ffs_rdfs(FFS_FSBTODB(fs, cgtod(fs, cg)), (int)fs->fs_cgsize, cgbuf, f);
	cgp = (struct cg *)cgbuf;
	if (!cg_chkmagic(cgp, 0))
		errx(EXIT_FAILURE, "cylinder group %u has a bad magic number", cg);
	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		errx(EXIT_FAILURE, "cylinder group %u is out of inodes (for inode %u): use a bigger image or -f", cg, ino);
	assert(isclr(cg_inosused(cgp, 0), cgino));
	setbit(cg_inosused(cgp, 0), cgino);
	cgp->cg_cs.cs_nifree--;
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	if (S_ISDIR(dp->di_mode)) {
		cgp->cg_cs.cs_ndir++;
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}
	/* the blocks of inodes of FFS2 are made as they are first used */
	initediblk = cgp->cg_initediblk;
	while (cgino + FFS_INOPB(fs) > initediblk && initediblk < cgp->cg_niblk) {
		memset(blk, 0, (size_t)fs->fs_bsize);
		dip = (struct ufs2_dinode *)blk;
		for (i = 0; i < (uint32_t)FFS_INOPB(fs); i++)
			dip[i].di_gen = (uint32_t)(random() / 2 + 1);
		ffs_wtfs(FFS_FSBTODB(fs, ino_to_fsba(fs, cg * fs->fs_ipg + initediblk)), fs->fs_bsize, blk, f);
		initediblk += FFS_INOPB(fs);
		cgp->cg_initediblk = initediblk;
	}
	ffs_wtfs(FFS_FSBTODB(fs, cgtod(fs, cg)), (int)fs->fs_cgsize, cgbuf, f);

	d = FFS_FSBTODB(fs, ino_to_fsba(fs, ino));
	ffs_rdfs(d, fs->fs_bsize, blk, f);
	((struct ufs2_dinode *)blk)[ino_to_fsbo(fs, ino)] = *dp;
	ffs_wtfs(d, fs->fs_bsize, blk, f);
	free(cgbuf);
	free(blk);
}

/*
 * Write an inode with its data: the bytes of fd (size of them), or of buf.
 * din has everything but the blocks.
 */
static void
write_file(struct ufs2_dinode *din, uint32_t ino, int fd, const char *name, const void *buf, fsinfo_t *f)
{
	struct fs *fs = f->superblock;
	ffs_opt_t *o = opts_of(f);
	struct vnode vp = { f };
	struct inode in;
	uint64_t size = din->di_size, done;
	char *chunkbuf = fd >= 0 ? emalloc((size_t)o->bsize) : NULL;

	memset(&in, 0, sizeof(in));
	in.i_fs = fs;
	in.i_devvp = &vp;
	in.i_number = ino;
	in.i_size = size;
	in.i_din.ffs2_din = *din;
	for (done = 0; done < size; ) {
		size_t chunk = (size_t)MIN(size - done, (uint64_t)o->bsize);
		const void *src = buf == NULL ? NULL : (const char *)buf + done;
		struct buf *bp;
		int e;

		if (fd >= 0) {
			ssize_t n = read(fd, chunkbuf, chunk);

			if (n != (ssize_t)chunk)
				errx(EXIT_FAILURE, "`%s' is shorter than it was (read %zd of %zu bytes at %ju)",
				    name, n, chunk, (uintmax_t)done);
			src = chunkbuf;
		}
		if ((e = ffs_balloc(&in, (off_t)done, (int)chunk, &bp)) != 0) {
			errno = e;
			err(EXIT_FAILURE, "No room in the image for `%s' (inode %u): give a bigger -s or -b", name, ino);
		}
		memcpy(bp->b_data, src, chunk);
		if ((e = bwrite(bp)) != 0) {
			errno = e;
			err(EXIT_FAILURE, "Writing `%s' (inode %u)", name, ino);
		}
		done += chunk;
	}
	free(chunkbuf);
	write_inode(&in.i_din.ffs2_din, ino, f);
}

static void
init_inode(struct ufs2_dinode *din, const fsnode *n)
{
	const struct stat *st = &n->inode->st;

	memset(din, 0, sizeof(*din));
	din->di_mode = st->st_mode;
	din->di_nlink = (int16_t)n->inode->nlink;
	din->di_uid = st->st_uid;
	din->di_gid = st->st_gid;
	din->di_gen = (uint32_t)(random() / 2 + 1);
	set_times(din, st);
}

/* one file that is not a directory */
static void
write_leaf(const fsnode *n, fsinfo_t *f)
{
	struct ufs2_dinode din;
	uint32_t ino = n->inode->ino;

	init_inode(&din, n);
	if (S_ISREG(n->type)) {
		int fd;

		din.di_size = (uint64_t)n->inode->st.st_size;
		if (din.di_size == 0) {
			write_inode(&din, ino, f);
			return;
		}
		if ((fd = open(n->path, O_RDONLY)) == -1)
			err(EXIT_FAILURE, "Can't open `%s'", n->path);
		write_file(&din, ino, fd, n->path, NULL, f);
		close(fd);
	} else if (S_ISLNK(n->type)) {
		size_t len = strlen(n->symlink);

		din.di_size = len;
		if (len < UFS2_MAXSYMLINKLEN) {
			memcpy(din.di_db, n->symlink, len);
			write_inode(&din, ino, f);
		} else
			write_file(&din, ino, -1, n->path, n->symlink, f);
	} else {
		if (S_ISBLK(n->type) || S_ISCHR(n->type))
			din.di_rdev = n->inode->st.st_rdev;
		write_inode(&din, ino, f);
	}
}

/* a directory and what is below it; its inode number is dir->inode->ino, its parent's pino */
static void
populate(fsnode *dir, uint32_t pino, fsinfo_t *f)
{
	fsnode *c;
	struct ufs2_dinode din;
	uint8_t *buf;
	size_t len;
	uint32_t subdirs = 0;

	for (c = dir->child; c != NULL; c = c->next) {
		if (c->inode->ino == 0)
			c->inode->ino = f->curinode++;
		if (c->type == S_IFDIR)
			subdirs++;
	}
	dir->inode->nlink = 2 + subdirs;
	for (c = dir->child; c != NULL; c = c->next)
		if (c->type != S_IFDIR && !c->inode->written) {
			c->inode->written = true;
			write_leaf(c, f);
		}
	for (c = dir->child; c != NULL; c = c->next)
		if (c->type == S_IFDIR)
			populate(c, dir->inode->ino, f);

	len = dir_pack(dir, dir->inode->ino, pino, NULL);
	buf = ecalloc(1, len);
	dir_pack(dir, dir->inode->ino, pino, buf);
	init_inode(&din, dir);
	din.di_size = len;
	write_file(&din, dir->inode->ino, -1, dir->path, buf, f);
	free(buf);
}

/* ---- the image ---- */

void
ffs_makefs(const char *image, fsnode *root, fsinfo_t *f)
{
	struct fs *fs;
	time_t tstamp = stamped ? stamp : time(NULL);

	validate(root, f);
	printf("makefs: %s: %jd bytes, %jd inodes\n", image, (intmax_t)f->size, (intmax_t)f->inodes);
	if ((f->fd = open(image, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
		err(EXIT_FAILURE, "Can't open `%s' for writing", image);
	if (ftruncate(f->fd, f->offset + f->size) == -1)
		err(EXIT_FAILURE, "Can't make `%s' %jd bytes long", image, (intmax_t)(f->offset + f->size));
	srandom((unsigned)tstamp);
	fs = ffs_mkfs(image, f, tstamp);
	f->superblock = fs;
	if ((off_t)(fs->fs_cstotal.cs_nifree + UFS_ROOTINO) < f->inodes)
		errx(EXIT_FAILURE, "`%s' has %jd inodes, %jd are needed: give a bigger -s, or a smaller -o density",
		    image, (intmax_t)(fs->fs_cstotal.cs_nifree + UFS_ROOTINO), (intmax_t)f->inodes);

	root->inode->ino = UFS_ROOTINO;
	f->curinode = UFS_ROOTINO + 1;
	populate(root, UFS_ROOTINO, f);

	fs->fs_fmod = 0;
	fs->fs_old_cstotal.cs_ndir = (int32_t)fs->fs_cstotal.cs_ndir;
	fs->fs_old_cstotal.cs_nbfree = (int32_t)fs->fs_cstotal.cs_nbfree;
	fs->fs_old_cstotal.cs_nifree = (int32_t)fs->fs_cstotal.cs_nifree;
	fs->fs_old_cstotal.cs_nffree = (int32_t)fs->fs_cstotal.cs_nffree;
	ffs_write_superblock(fs, f);
	if (close(f->fd) == -1)
		err(EXIT_FAILURE, "Closing `%s'", image);
	f->fd = -1;
	printf("makefs: %s is made\n", image);
}

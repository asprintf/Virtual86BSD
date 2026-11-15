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

/* ffs.c - a read-only FFS2 for the loader: the super block, the inodes, the
   block map with its indirect blocks, directories and symbolic links.  The
   structure of the files follows the stand-alone readers of the BSDs
   (libsa's ufs.c); the disk formats are those of sys/ufs/ffs/fs.h. */
#include "boot.h"
#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>

_Static_assert(sizeof(struct ufs2_dinode) == 256, "ufs2_dinode");
_Static_assert(offsetof(struct fs, fs_magic) == 1372, "fs_magic");

#define FMT 0170000
#define FDIR 0040000
#define FLNK 0120000
#define MAXBLK 65536

static uint8_t sbbuf[SBLOCKSIZE];
static struct fs *fs = (struct fs *)sbbuf;
static int fdisk = -1;
static uint32_t fstart, fsectors;
static uint8_t blk[MAXBLK], ind[MAXBLK];
static int64_t blkfsb = -1, indfsb = -1;

/* bytes (a multiple of 512) from the fragment fsb */
static int rdfrags(int64_t fsb, uint32_t bytes, void *buf) {
	uint64_t sec = (uint64_t)fsb << fs->fs_fsbtodb;
	if (sec + bytes / 512 > fsectors)
		return -1;
	return disk_read(fdisk, fstart + (uint32_t)sec, bytes / 512, buf);
}

int ffs_mount(int d, uint32_t start, uint32_t sectors) {
	fdisk = -1;
	blkfsb = indfsb = -1;
	if (start + 128 + SBLOCKSIZE / 512 > start + sectors || disk_read(d, start + SBLOCK_UFS2 / 512, SBLOCKSIZE / 512, sbbuf))
		return -1;
	if (fs->fs_magic != FS_UFS2_MAGIC)
		return -1;
	uint32_t bs = (uint32_t)fs->fs_bsize, fsz = (uint32_t)fs->fs_fsize;
	if (bs < 4096 || bs > MAXBLK || (bs & (bs - 1)) || fsz < 512 || fsz > bs || (fsz & (fsz - 1)) || fs->fs_frag != (int32_t)(bs / fsz) ||
	    fs->fs_ipg <= 0 || fs->fs_fpg <= 0 || fs->fs_inopb <= 0 || fs->fs_bshift < 9 || fs->fs_fsbtodb < 0)
		return -1;
	fdisk = d;
	fstart = start;
	fsectors = sectors;
	return 0;
}

static int read_inode(uint32_t ino, struct ffsfile *f) {
	uint32_t cg = ino / (uint32_t)fs->fs_ipg;
	uint64_t fsb = (uint64_t)cg * (uint32_t)fs->fs_fpg + (uint64_t)(int64_t)fs->fs_cgoffset * (cg & ~(uint32_t)fs->fs_cgmask) +
	    (uint32_t)fs->fs_iblkno + (((ino % (uint32_t)fs->fs_ipg) / (uint32_t)fs->fs_inopb) << fs->fs_fragshift);
	uint32_t idx = ino % (uint32_t)fs->fs_inopb;
	uint8_t sec[512];
	uint64_t s = (fsb << fs->fs_fsbtodb) + idx * 256 / 512;
	if (s >= fsectors || disk_read(fdisk, fstart + (uint32_t)s, 1, sec))
		return -1;
	memcpy(f->di, sec + (idx * 256 % 512), 256);
	const struct ufs2_dinode *di = (const struct ufs2_dinode *)f->di;
	f->ino = ino;
	f->mode = di->di_mode;
	f->size = di->di_size >> 32 ? 0xffffffffU : (uint32_t)di->di_size;
	return 0;
}

/* the fragment of the indirect block fsb that holds entry idx */
static int64_t indir(int64_t fsb, uint32_t idx) {
	if (fsb <= 0)
		return 0;
	if (fsb != indfsb) {
		if (rdfrags(fsb, (uint32_t)fs->fs_bsize, ind))
			return -1;
		indfsb = fsb;
	}
	int64_t v;
	memcpy(&v, ind + idx * 8, 8);
	return v;
}

static int64_t bmap(const struct ufs2_dinode *di, uint32_t lbn) {
	if (lbn < NDADDR)
		return di->di_db[lbn];
	lbn -= NDADDR;
	uint32_t n = (uint32_t)fs->fs_bsize / 8;
	if (lbn < n)
		return indir(di->di_ib[0], lbn);
	lbn -= n;
	if (lbn < n * n) {
		int64_t b = indir(di->di_ib[1], lbn / n);
		return b > 0 ? indir(b, lbn % n) : b;
	}
	return -1;
}

int ffs_read(struct ffsfile *f, void *dst, uint32_t off, uint32_t len) {
	const struct ufs2_dinode *di = (const struct ufs2_dinode *)f->di;
	uint8_t *out = dst;
	uint32_t bs = (uint32_t)fs->fs_bsize, done = 0;
	if (off >= f->size)
		return 0;
	if (len > f->size - off)
		len = f->size - off;
	while (len) {
		uint32_t lbn = off >> fs->fs_bshift, bo = off & (bs - 1);
		uint32_t n = bs - bo < len ? bs - bo : len;
		int64_t fsb = bmap(di, lbn);
		if (fsb < 0)
			return -1;
		if (fsb == 0) {
			memset(out, 0, n);
		} else {
			/* the last block of a file may be fragments only */
			uint32_t bytes = bs;
			if (lbn < NDADDR && f->size < (lbn + 1) << fs->fs_bshift)
				bytes = (f->size - (lbn << fs->fs_bshift) + (uint32_t)fs->fs_fsize - 1) & ~((uint32_t)fs->fs_fsize - 1);
			if (fsb != blkfsb) {
				blkfsb = -1;
				if (rdfrags(fsb, bytes, blk))
					return -1;
				blkfsb = bytes == bs ? fsb : -1;
			}
			memcpy(out, blk + bo, n);
		}
		out += n;
		off += n;
		len -= n;
		done += n;
	}
	return (int)done;
}

/* the entries of a directory: *pos is the offset of the next one (0 at the
   start); 1 for an entry, 0 at the end */
int ffs_next(struct ffsfile *dir, uint32_t *pos, char *name, int *isdir) {
	static uint8_t chunk[512];
	static uint32_t cpos;
	if (*pos == 0)
		cpos = ~0u;
	while (*pos < dir->size) {
		uint32_t base = *pos & ~511u;
		if (base != cpos) {
			if (ffs_read(dir, chunk, base, 512) < 0)
				return 0;
			cpos = base;
		}
		const struct direct *d = (const struct direct *)(chunk + (*pos & 511));
		if (d->d_reclen < 8 || (*pos & 511) + d->d_reclen > 512) {
			*pos = base + 512;
			continue;
		}
		*pos += d->d_reclen;
		if (!d->d_ino)
			continue;
		memcpy(name, d->d_name, d->d_namlen);
		name[d->d_namlen] = 0;
		*isdir = d->d_type == DT_DIR;
		if (d->d_type == DT_UNKNOWN) {
			struct ffsfile t;
			*isdir = !read_inode(d->d_ino, &t) && (t.mode & FMT) == FDIR;
		}
		return 1;
	}
	return 0;
}

static int find(struct ffsfile *dir, const char *name, size_t len, uint32_t *ino) {
	uint32_t pos = 0;
	static uint8_t chunk[512];
	while (pos < dir->size) {
		uint32_t base = pos & ~511u;
		if (ffs_read(dir, chunk, base, 512) < 0)
			return -1;
		while (pos < base + 512 && pos < dir->size) {
			const struct direct *d = (const struct direct *)(chunk + (pos & 511));
			if (d->d_reclen < 8 || (pos & 511) + d->d_reclen > 512)
				break;
			pos += d->d_reclen;
			if (d->d_ino && d->d_namlen == len && !memcmp(d->d_name, name, len)) {
				*ino = d->d_ino;
				return 0;
			}
		}
		pos = base + 512;
	}
	return -1;
}

static int walk(const char *path, const struct ffsfile *start, struct ffsfile *res, int depth) {
	struct ffsfile cur = *start, dir;
	const char *p = path;
	for (;;) {
		while (*p == '/')
			p++;
		if (!*p)
			break;
		const char *e = p;
		while (*e && *e != '/')
			e++;
		if ((cur.mode & FMT) != FDIR)
			return -1;
		dir = cur;
		uint32_t ino;
		if (find(&dir, p, (size_t)(e - p), &ino) || read_inode(ino, &cur))
			return -1;
		if ((cur.mode & FMT) == FLNK) {
			char link[256], np[512];
			if (depth >= 8 || cur.size == 0 || cur.size >= sizeof(link))
				return -1;
			const struct ufs2_dinode *di = (const struct ufs2_dinode *)cur.di;
			if (cur.size < (uint32_t)fs->fs_maxsymlinklen)
				memcpy(link, di->di_db, cur.size);
			else if (ffs_read(&cur, link, 0, cur.size) != (int)cur.size)
				return -1;
			link[cur.size] = 0;
			if (strlen(link) + strlen(e) + 2 > sizeof(np))
				return -1;
			snprintf(np, sizeof(np), "%s/%s", link, e);
			struct ffsfile base;
			if (link[0] == '/' ? read_inode(ROOTINO, &base) : (base = dir, 0))
				return -1;
			return walk(np, &base, res, depth + 1);
		}
		p = e;
	}
	*res = cur;
	return 0;
}

int ffs_lookup(const char *path, struct ffsfile *f) {
	struct ffsfile root;
	if (fdisk < 0 || read_inode(ROOTINO, &root))
		return -1;
	return walk(path, &root, f, 0);
}

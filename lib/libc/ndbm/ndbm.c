/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

/* <ndbm.h>: dynamic hashing as in sdbm (Per-Aake Larson's scheme as
 * Ozan Yigit used it).  file.pag holds pages of PBLKSIZ bytes; file.dir
 * a bit per node of the (implicit) binary trie of page splits.  A key's
 * hash selects its page by walking the trie from bit 0 while the nodes
 * are split.  A full page splits in two by the next hash bit.
 *
 * Page layout: unsigned short n, then n offsets; item i (keys at odd i,
 * values at even i) occupies [off[i], off[i-1]) with off[0] = PBLKSIZ,
 * so the data grow down from the end of the page.
 * A key and its value together may take PAIRMAX bytes.
 * Pointers returned by fetch/firstkey/nextkey point into the page buffer
 * and stay valid until the next call on the same DBM. */
#include <ndbm.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PBLKSIZ 4096
#define DBLKSIZ 4096
#define PAIRMAX (PBLKSIZ - 16)
#define MAXBITS 31            /* trie depth; page numbers stay < 2^31 */

struct __dbm {
	int pagf, dirf;
	int rdonly, err;
	unsigned long maxbno;     /* bits in the directory file */
	unsigned long curbit, hmask;
	long pagbno;              /* page in pagbuf, -1 none */
	long dirbno;              /* directory block in dirbuf, -1 none */
	long blkptr;              /* iteration: page */
	int keyptr;               /* iteration: key number in it */
	unsigned short pag[PBLKSIZ / 2];
	unsigned char dir[DBLKSIZ];
};

#define PAGE(db) ((unsigned char *)(db)->pag)

static unsigned long hash(const datum *d)
{
	const unsigned char *p = d->dptr;
	unsigned long h = 0;
	size_t n = d->dsize;
	while (n--)
		h = *p++ + (h << 6) + (h << 16) - h;
	return h;
}

/* --- pages --- */

static int pagevalid(const unsigned short *ino)
{
	unsigned n = ino[0], i, prev = PBLKSIZ;
	if (n & 1 || (n + 1) * 2 > PBLKSIZ)
		return 0;
	for (i = 1; i <= n; i++) {
		if (ino[i] > prev || ino[i] < (n + 1) * 2)
			return 0;
		prev = ino[i];
	}
	return 1;
}

static size_t room(const unsigned short *ino)
{
	unsigned n = ino[0];
	unsigned end = n ? ino[n] : PBLKSIZ;
	return end - (n + 1) * 2;
}

static int fits(const unsigned short *ino, size_t need)
{
	return room(ino) >= need + 4;
}

static datum item(unsigned char *pg, int i)
{
	unsigned short *ino = (unsigned short *)pg;
	datum d;
	unsigned hi = i == 1 ? PBLKSIZ : ino[i - 1];
	d.dptr = pg + ino[i];
	d.dsize = hi - ino[i];
	return d;
}

static void putpair(unsigned char *pg, datum k, datum v)
{
	unsigned short *ino = (unsigned short *)pg;
	unsigned n = ino[0];
	unsigned off = n ? ino[n] : PBLKSIZ;
	off -= (unsigned)k.dsize;
	memcpy(pg + off, k.dptr, k.dsize);
	ino[++n] = (unsigned short)off;
	off -= (unsigned)v.dsize;
	memcpy(pg + off, v.dptr, v.dsize);
	ino[++n] = (unsigned short)off;
	ino[0] = (unsigned short)n;
}

/* the item number of key k, or 0 */
static int findkey(unsigned char *pg, datum k)
{
	unsigned short *ino = (unsigned short *)pg;
	int i;
	datum d;
	for (i = 1; i < ino[0]; i += 2) {
		d = item(pg, i);
		if (d.dsize == k.dsize && !memcmp(d.dptr, k.dptr, k.dsize))
			return i;
	}
	return 0;
}

/* remove the pair at item i (a key) and close the gap */
static void delpair(unsigned char *pg, int i)
{
	unsigned short *ino = (unsigned short *)pg;
	unsigned n = ino[0], top = i == 1 ? PBLKSIZ : ino[i - 1];
	unsigned bot = ino[i + 1], gap = top - bot, end = ino[n];
	unsigned j;
	memmove(pg + end + gap, pg + end, bot - end);
	for (j = i + 2; j <= n; j++)
		ino[j - 2] = (unsigned short)(ino[j] + gap);
	ino[0] = (unsigned short)(n - 2);
}

static int readpage(DBM *db, long b, unsigned short *buf)
{
	ssize_t r = pread(db->pagf, buf, PBLKSIZ, (off_t)b * PBLKSIZ);
	if (r < 0)
		return -1;
	if (r < PBLKSIZ)
		memset((char *)buf + r, 0, PBLKSIZ - r);
	if (!pagevalid(buf)) {
		errno = EILSEQ;
		return -1;
	}
	return (int)r;
}

static int writepage(DBM *db, long b, const unsigned short *buf)
{
	return pwrite(db->pagf, buf, PBLKSIZ, (off_t)b * PBLKSIZ) == PBLKSIZ ? 0 : -1;
}

static int loadpage(DBM *db, long b)
{
	if (db->pagbno == b)
		return 0;
	db->pagbno = -1;
	if (readpage(db, b, db->pag) < 0)
		return -1;
	db->pagbno = b;
	return 0;
}

/* --- directory --- */

static int getdbit(DBM *db, unsigned long bit)
{
	long blk = (long)(bit / 8 / DBLKSIZ);
	if (db->dirbno != blk) {
		ssize_t r = pread(db->dirf, db->dir, DBLKSIZ, (off_t)blk * DBLKSIZ);
		if (r < 0)
			return -1;
		memset(db->dir + r, 0, DBLKSIZ - r);
		db->dirbno = blk;
	}
	return db->dir[bit / 8 % DBLKSIZ] >> (bit % 8) & 1;
}

static int setdbit(DBM *db, unsigned long bit)
{
	unsigned long byte = bit / 8;
	if (getdbit(db, bit) < 0)
		return -1;
	db->dir[byte % DBLKSIZ] |= (unsigned char)(1 << (bit % 8));
	if (bit >= db->maxbno)
		db->maxbno = (byte + 1) * 8;
	if (pwrite(db->dirf, db->dir + byte % DBLKSIZ, 1, (off_t)byte) != 1)
		return -1;
	return 0;
}

/* make the page for hash h current */
static int getpage(DBM *db, unsigned long h)
{
	unsigned long dbit = 0;
	int hbit = 0, b;
	while (dbit < db->maxbno && hbit < MAXBITS) {
		if ((b = getdbit(db, dbit)) < 0)
			return -1;
		if (!b)
			break;
		dbit = 2 * dbit + (h >> hbit++ & 1 ? 2 : 1);
	}
	db->curbit = dbit;
	db->hmask = (1UL << hbit) - 1;
	return loadpage(db, (long)(h & db->hmask));
}

/* split the current page until a pair of `need' bytes with hash h fits */
static int makeroom(DBM *db, unsigned long h, size_t need)
{
	unsigned short newp[PBLKSIZ / 2], old[PBLKSIZ / 2];
	unsigned char *pg = PAGE(db);
	unsigned long bit;
	long newb;
	int i;
	while (!fits(db->pag, need)) {
		if (db->hmask >= (1UL << (MAXBITS - 1)) - 1) {
			errno = ENOSPC;
			return -1;
		}
		bit = db->hmask + 1;
		newb = (long)((h & db->hmask) | bit);
		memcpy(old, db->pag, PBLKSIZ);
		memset(db->pag, 0, PBLKSIZ);
		memset(newp, 0, PBLKSIZ);
		for (i = 1; i < old[0]; i += 2) {
			datum k = item((unsigned char *)old, i), v = item((unsigned char *)old, i + 1);
			putpair(hash(&k) & bit ? (unsigned char *)newp : pg, k, v);
		}
		if (writepage(db, newb, newp) < 0 || writepage(db, db->pagbno, db->pag) < 0
		    || setdbit(db, db->curbit) < 0)
			return -1;
		db->curbit = 2 * db->curbit + (h & bit ? 2 : 1);
		db->hmask |= bit;
		if (h & bit) {
			memcpy(db->pag, newp, PBLKSIZ);
			db->pagbno = newb;
		}
	}
	return 0;
}

/* --- interface --- */

DBM *dbm_open(const char *file, int flags, mode_t mode)
{
	DBM *db;
	size_t n = strlen(file);
	char *name;
	off_t size;
	if (!(name = malloc(n + 5)))
		return 0;
	if (!(db = malloc(sizeof *db))) {
		free(name);
		return 0;
	}
	if ((flags & O_ACCMODE) == O_WRONLY)
		flags = (flags & ~O_ACCMODE) | O_RDWR;
	db->rdonly = (flags & O_ACCMODE) == O_RDONLY;
	memcpy(name, file, n);
	strcpy(name + n, ".pag");
	db->pagf = open(name, flags, mode);
	strcpy(name + n, ".dir");
	db->dirf = db->pagf < 0 ? -1 : open(name, flags, mode);
	free(name);
	if (db->dirf < 0 || (size = lseek(db->dirf, 0, SEEK_END)) < 0) {
		int e = errno;
		if (db->pagf >= 0)
			close(db->pagf);
		if (db->dirf >= 0)
			close(db->dirf);
		free(db);
		errno = e;
		return 0;
	}
	db->maxbno = (unsigned long)size * 8;
	db->err = 0;
	db->pagbno = db->dirbno = -1;
	db->blkptr = 0;
	db->keyptr = 0;
	return db;
}

void dbm_close(DBM *db)
{
	close(db->pagf);
	close(db->dirf);
	free(db);
}

int dbm_error(DBM *db)
{
	return db->err;
}

int dbm_clearerr(DBM *db)
{
	db->err = 0;
	return 0;
}

static datum nulldatum(void)
{
	datum d = { 0, 0 };
	return d;
}

datum dbm_fetch(DBM *db, datum key)
{
	int i;
	if (getpage(db, hash(&key)) < 0) {
		db->err = 1;
		return nulldatum();
	}
	if (!(i = findkey(PAGE(db), key)))
		return nulldatum();
	return item(PAGE(db), i + 1);
}

int dbm_delete(DBM *db, datum key)
{
	int i;
	if (db->rdonly) {
		errno = EPERM;
		return -1;
	}
	if (getpage(db, hash(&key)) < 0) {
		db->err = 1;
		return -1;
	}
	if (!(i = findkey(PAGE(db), key)))
		return -1;
	delpair(PAGE(db), i);
	if (writepage(db, db->pagbno, db->pag) < 0) {
		db->err = 1;
		return -1;
	}
	return 0;
}

int dbm_store(DBM *db, datum key, datum val, int mode)
{
	unsigned long h = hash(&key);
	size_t need = key.dsize + val.dsize;
	int i;
	if (db->rdonly) {
		errno = EPERM;
		return -1;
	}
	if (need > PAIRMAX || (!key.dptr && key.dsize) || (!val.dptr && val.dsize)) {
		errno = EINVAL;
		return -1;
	}
	if (getpage(db, h) < 0)
		goto fail;
	if ((i = findkey(PAGE(db), key))) {
		if (mode == DBM_INSERT)
			return 1;
		delpair(PAGE(db), i);
	}
	if (!fits(db->pag, need) && makeroom(db, h, need) < 0) {
		db->pagbno = -1;      /* the buffer may hold an unwritten split */
		goto fail;
	}
	putpair(PAGE(db), key, val);
	if (writepage(db, db->pagbno, db->pag) < 0)
		goto fail;
	return 0;
fail:
	db->err = 1;
	return -1;
}

static datum nextkey(DBM *db)
{
	int r;
	for (;;) {
		if (db->pagbno != db->blkptr) {
			db->pagbno = -1;
			if ((r = readpage(db, db->blkptr, db->pag)) < 0) {
				db->err = 1;
				return nulldatum();
			}
			if (!r)
				return nulldatum();   /* past the end of the file */
			db->pagbno = db->blkptr;
		}
		if (++db->keyptr * 2 <= db->pag[0])
			return item(PAGE(db), db->keyptr * 2 - 1);
		db->blkptr++;
		db->keyptr = 0;
	}
}

datum dbm_firstkey(DBM *db)
{
	db->blkptr = 0;
	db->keyptr = 0;
	return nextkey(db);
}

datum dbm_nextkey(DBM *db)
{
	return nextkey(db);
}

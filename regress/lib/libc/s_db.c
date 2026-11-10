/* Self-checking: <ndbm.h>, the balance of tsearch's tree, <dlfcn.h> (the
 * program's own symbols), encrypt/setkey (FIPS 81 vectors), and that
 * <cpio.h>, <tar.h> and <search.h> define what they must. */
#define _XOPEN_SOURCE 800
#include <ndbm.h>
#include <search.h>
#include <dlfcn.h>
#include <cpio.h>
#include <tar.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "out.h"

int s_db_global = 5;
int main(void);

/* --- ndbm --- */

static size_t mkval(unsigned i, char *v)
{
	size_t n = (i * 2654435761u) % 300, j;
	for (j = 0; j < n; j++)
		v[j] = (char)(i + j);
	return n;
}

static datum dat(void *p, size_t n)
{
	datum d;
	d.dptr = p;
	d.dsize = n;
	return d;
}

static void ndbm(void)
{
	enum { N = 20000 };
	char base[] = "/tmp/s_db.XXXXXX", name[64], k[32], v[400];
	static char big[5000], seen[N];
	DBM *db;
	datum d;
	int fd, i, n, ok;
	struct stat st;

	fd = mkstemp(base);
	close(fd);
	unlink(base);
	CHECK((db = dbm_open(base, O_RDWR | O_CREAT, 0600)) != 0);
	if (!db)
		return;
	for (i = 0, ok = 1; i < N; i++) {
		size_t kl = (size_t)(stpcpy(k, "key") - k);
		kl += (size_t)(stpcpy(k + kl, "") - (k + kl));
		{ int x = i, j = 0; char t[12]; do t[j++] = (char)('0' + x % 10); while (x /= 10); while (j) k[kl++] = t[--j]; }
		if (dbm_store(db, dat(k, kl), dat(v, mkval((unsigned)i, v)), DBM_INSERT) != 0)
			ok = 0;
	}
	CHECK(ok);
	CHECK(dbm_store(db, dat("key7", 4), dat("x", 1), DBM_INSERT) == 1);
	CHECK(dbm_store(db, dat("key7", 4), dat("seven", 5), DBM_REPLACE) == 0);
	d = dbm_fetch(db, dat("key7", 4));
	CHECK(d.dsize == 5 && !memcmp(d.dptr, "seven", 5));
	CHECK(dbm_store(db, dat("key7", 4), dat(v, mkval(7, v)), DBM_REPLACE) == 0);
	/* big pairs and the limit */
	memset(big, 'b', sizeof big);
	CHECK(dbm_store(db, dat("big1", 4), dat(big, 4000), DBM_INSERT) == 0);
	CHECK(dbm_store(db, dat("big2", 4), dat(big, 4076), DBM_INSERT) == 0);
	errno = 0;
	CHECK(dbm_store(db, dat("big3", 4), dat(big, 4900), DBM_INSERT) < 0 && errno == EINVAL);
	CHECK(dbm_error(db) == 0);
	CHECK(dbm_store(db, dat("", 0), dat("empty key", 9), DBM_INSERT) == 0);
	CHECK(dbm_store(db, dat("emptyval", 8), dat("", 0), DBM_INSERT) == 0);
	d = dbm_fetch(db, dat("", 0));
	CHECK(d.dsize == 9 && !memcmp(d.dptr, "empty key", 9));
	d = dbm_fetch(db, dat("emptyval", 8));
	CHECK(d.dptr != 0 && d.dsize == 0);
	d = dbm_fetch(db, dat("big2", 4));
	CHECK(d.dsize == 4076 && !memcmp(d.dptr, big, 4076));
	/* fetch everything, delete every third */
	for (i = 0, ok = 1; i < N; i++) {
		int x = i, j = 0, kl = 3; char t[12];
		memcpy(k, "key", 3);
		do t[j++] = (char)('0' + x % 10); while (x /= 10);
		while (j) k[kl++] = t[--j];
		d = dbm_fetch(db, dat(k, (size_t)kl));
		if (!d.dptr || d.dsize != mkval((unsigned)i, v) || memcmp(d.dptr, v, d.dsize))
			ok = 0;
		if (i % 3 == 0 && dbm_delete(db, dat(k, (size_t)kl)) != 0)
			ok = 0;
	}
	CHECK(ok);
	CHECK(dbm_delete(db, dat("key0", 4)) < 0);
	CHECK(dbm_fetch(db, dat("key0", 4)).dptr == 0);
	CHECK(dbm_fetch(db, dat("nokey", 5)).dptr == 0);
	/* iterate */
	n = 0;
	ok = 1;
	for (d = dbm_firstkey(db); d.dptr; d = dbm_nextkey(db)) {
		n++;
		if (d.dsize > 3 && !memcmp(d.dptr, "key", 3)) {
			int x = 0;
			size_t j;
			for (j = 3; j < d.dsize; j++) x = x * 10 + ((char *)d.dptr)[j] - '0';
			if (x < 0 || x >= N || x % 3 == 0 || seen[x]) ok = 0;
			else seen[x] = 1;
		}
	}
	CHECK(ok);
	CHECK(n == N - (N + 2) / 3 + 4);
	CHECK(dbm_error(db) == 0 && dbm_clearerr(db) == 0);
	dbm_close(db);
	memcpy(name, base, strlen(base));
	strcpy(name + strlen(base), ".dir");
	CHECK(stat(name, &st) == 0);
	strcpy(name + strlen(base), ".pag");
	CHECK(stat(name, &st) == 0 && st.st_size > 0);
	/* again, read-only */
	CHECK((db = dbm_open(base, O_RDONLY, 0)) != 0);
	if (db) {
		d = dbm_fetch(db, dat("key19999", 8));
		CHECK(d.dptr && d.dsize == mkval(19999, v) && !memcmp(d.dptr, v, d.dsize));
		CHECK(dbm_store(db, dat("a", 1), dat("b", 1), DBM_INSERT) < 0);
		CHECK(dbm_delete(db, dat("key1", 4)) < 0);
		for (n = 0, d = dbm_firstkey(db); d.dptr; d = dbm_nextkey(db)) n++;
		CHECK(n == N - (N + 2) / 3 + 4);
		dbm_close(db);
	}
	errno = 0;
	CHECK(dbm_open("/nonexistent/dir/db", O_RDWR, 0) == 0 && errno == ENOENT);
	unlink(name);
	strcpy(name + strlen(base), ".dir");
	unlink(name);
}

/* --- tsearch balance --- */

static int cmpint(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

static int maxdepth, nodes, last, sorted;
static void walk(const void *n, VISIT v, int depth)
{
	if (depth > maxdepth) maxdepth = depth;
	if (v == postorder || v == leaf) {
		int x = **(int *const *)n;
		if (nodes && x <= last) sorted = 0;
		last = x;
		nodes++;
	}
}

static void tree(void)
{
	enum { N = 100000 };
	static int vals[N];
	void *root = 0;
	int i, distinct = 0, ok = 1;
	for (i = 0; i < N; i++)
		vals[i] = (int)(rng() % (N * 4));
	for (i = 0; i < N; i++) {
		void *p = tsearch(&vals[i], &root, cmpint);
		if (*(int **)p == &vals[i]) distinct++;
	}
	maxdepth = nodes = 0; sorted = 1;
	twalk(root, walk);
	CHECK(nodes == distinct && sorted);
	CHECK(maxdepth <= 24);       /* AVL: < 1.44 log2(n + 2) */
	for (i = 0; i < N; i += 2) {
		void *had = tfind(&vals[i], &root, cmpint);
		void *p = tdelete(&vals[i], &root, cmpint);
		if ((had != 0) != (p != 0)) ok = 0;
		if (had) distinct--;
	}
	CHECK(ok);
	maxdepth = nodes = 0; sorted = 1;
	twalk(root, walk);
	CHECK(nodes == distinct && sorted && maxdepth <= 24);
	/* tdelete gives the parent */
	{
		static int a = 1, b = 2;
		void *r = 0, *pa;
		pa = tsearch(&a, &r, cmpint);
		tsearch(&b, &r, cmpint);
		CHECK(tdelete(&b, &r, cmpint) == pa);
		CHECK(tdelete(&a, &r, cmpint) != 0 && r == 0);
	}
	{
		posix_tnode *t = 0;
		CHECK(tfind(&i, &t, cmpint) == 0);
	}
}

/* --- dlfcn --- */

static void *thr(void *p)
{
	(void)p;
	if (dlsym(dlopen(0, RTLD_NOW), "no such symbol"))
		return (void *)1;
	return dlerror();
}

static void dl(void)
{
	void *h = dlopen(0, RTLD_LAZY), *r;
	Dl_info_t info;
	int x;
	pthread_t t;
	CHECK(h != 0);
	CHECK(dlerror() == 0);
	if (!dlsym(h, "main")) {
		/* stripped (or no symbol table written): nothing to check */
		CHECK(dlerror() != 0);
	} else {
		CHECK(dlsym(h, "main") == (void *)main);
		CHECK(dlsym(h, "s_db_global") == (void *)&s_db_global);
		CHECK(dlsym(h, "malloc") == (void *)malloc);
		CHECK(dladdr((char *)(void *)main + 1, &info) != 0);
		CHECK(info.dli_sname && !strcmp(info.dli_sname, "main"));
		CHECK(info.dli_saddr == (void *)main);
		CHECK(info.dli_fname && *info.dli_fname);
		CHECK((char *)info.dli_fbase <= (char *)(void *)main);
		CHECK(dladdr(&s_db_global, &info) != 0 && info.dli_sname && !strcmp(info.dli_sname, "s_db_global"));
	}
	CHECK(dlerror() == 0);
	CHECK(dlsym(h, "no such symbol") == 0);
	CHECK(dlerror() != 0);
	CHECK(dlerror() == 0);
	CHECK(dladdr(&x, &info) == 0);
	CHECK(dlerror() != 0);
	CHECK(dlopen("libm.so.6", RTLD_NOW) == 0);
	CHECK(dlerror() != 0);
	CHECK(dlopen(0, 0) == 0 && dlerror() != 0);
	/* errors are per thread */
	CHECK(pthread_create(&t, 0, thr, 0) == 0);
	CHECK(pthread_join(t, &r) == 0 && r != 0 && r != (void *)1);
	CHECK(dlerror() == 0);
	CHECK(dlclose(h) == 0);
	CHECK(dlclose(&x) != 0 && dlerror() != 0);
}

/* --- encrypt, setkey --- */

static void bits(char *b, unsigned long long v)
{
	int i;
	for (i = 0; i < 64; i++) b[i] = (char)(v >> (63 - i) & 1);
}

static unsigned long long val(const char *b)
{
	unsigned long long v = 0;
	int i;
	for (i = 0; i < 64; i++) v = v << 1 | (unsigned)(b[i] & 1);
	return v;
}

static void des(void)
{
	static const unsigned long long t[][3] = {   /* key, plain, cipher */
		{ 0x133457799BBCDFF1ULL, 0x0123456789ABCDEFULL, 0x85E813540F0AB405ULL },
		{ 0x0123456789ABCDEFULL, 0x4E6F772069732074ULL, 0x3FA40E8A984D4815ULL },
		{ 0x0123456789ABCDEFULL, 0x68652074696D6520ULL, 0x6A271787AB8883F9ULL },
		{ 0x0123456789ABCDEFULL, 0x666F7220616C6C20ULL, 0x893D51EC4B563B53ULL },
		{ 0x0101010101010101ULL, 0x95F8A5E5DD31D900ULL, 0x8000000000000000ULL },
	};
	char k[64], b[64];
	unsigned i;
	for (i = 0; i < sizeof t / sizeof *t; i++) {
		bits(k, t[i][0]);
		setkey(k);
		bits(b, t[i][1]);
		errno = 0;
		encrypt(b, 0);
		CHECK(val(b) == t[i][2] && errno == 0);
		encrypt(b, 1);
		CHECK(val(b) == t[i][1]);
	}
}

static void constants(void)
{
	static const char m[] = MAGIC, tm[TMAGLEN] = TMAGIC, tv[TVERSLEN + 1] = TVERSION;
	CHECK(!strcmp(m, "070707") && !strcmp(tm, "ustar") && !strcmp(tv, "00"));
	CHECK(C_ISDIR == 040000 && C_ISLNK == 0120000 && C_ISCTG == 0110000 && C_IRUSR == 0400);
	CHECK(REGTYPE == '0' && AREGTYPE == 0 && CONTTYPE == '7' && TSVTX == 01000 && TOEXEC == 1);
	{
		ENTRY e;
		ACTION a = ENTER;
		VISIT v = leaf;
		e.key = "k";
		e.data = 0;
		CHECK(a == 1 && v == 3 && FIND == 0 && preorder == 0 && e.key[0] == 'k');
	}
}

int main(void)
{
	ndbm();
	tree();
	dl();
	des();
	constants();
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}

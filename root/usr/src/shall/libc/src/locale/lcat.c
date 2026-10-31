/* Loading category data: the built-in locales, or the files of a locale
 * directory in the format of the GNU C library. ISO C (setlocale) uses
 * this, so only reserved names: system calls directly. */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "locale_impl.h"
#include "syscall.h"
#include "lock.h"
#include "libc.h"
#include "../iconv/iconv_impl.h"

static const char *const catnames[6] = {
	"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES/SYS_LC_MESSAGES",
};

static struct __lcat *cache;
static volatile int lock;

/* "en_US.UTF-8@euro" -> "en_US.utf8@euro", as glibc's localedef names
 * its directories; 0 if nothing changes */
static int normalize(const char *name, char *out, size_t size)
{
	const char *dot = strchr(name, '.'), *at, *p;
	size_t k, digits = 1;
	char *o;
	if (!dot) return 0;
	at = strchr(dot, '@');
	if (!at) at = dot + strlen(dot);
	if ((size_t)(dot - name) + 1 + (size_t)(at - dot) + 3 + strlen(at) >= size) return 0;
	memcpy(out, name, (size_t)(dot - name) + 1);
	o = out + (dot - name) + 1;
	for (p = dot + 1; p < at; p++) if (*p >= 'A' && *p <= 'z' && (*p >= 'a' || *p <= 'Z')) digits = 0;
	if (digits) { memcpy(o, "iso", 3); o += 3; }
	for (p = dot + 1; p < at; p++) {
		if (*p >= 'A' && *p <= 'Z') *o++ = (char)(*p + 32);
		else if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) *o++ = *p;
	}
	k = strlen(at);
	memcpy(o, at, k + 1);
	return strcmp(out, name) != 0;
}

/* a file, mapped; 0 if absent or unreadable */
static void *mapfile(const char *path, size_t *size)
{
	struct stat st;
	long fd = __syscall4(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	void *m;
	if (fd < 0) return 0;
	if (__syscall2(SYS_fstat, fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size < 8 || st.st_size > (1L << 30)) {
		__syscall1(SYS_close, fd);
		return 0;
	}
	m = (void *)__syscall6x(SYS_mmap, 0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	__syscall1(SYS_close, fd);
	if ((unsigned long)m > -4096UL) return 0;
	*size = (size_t)st.st_size;
	return m;
}

/* names of codesets compare without case, '-' and '_' */
static int ieq(const char *a, const char *b)
{
	for (;;) {
		int x, y;
		while (*a == '-' || *a == '_') a++;
		while (*b == '-' || *b == '_') b++;
		x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
		y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
		if (x != y) return 0;
		if (!x) return 1;
		a++; b++;
	}
}

static int isutf8(const char *s) { return ieq(s, "UTF-8"); }

static int ctype_setup(struct __lcat *c)
{
	static const char *const std[W_NSTD] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph",
		"lower", "print", "punct", "space", "upper", "xdigit" };
	const char *cs = __lc_str(c, 14), *p;
	unsigned i, j;
	if (c->n < 72 || !c->elem[13]) return -1;
	memcpy(&c->mbmax, c->elem[13], 4);
	c->classnames = c->elem[10];
	c->mapnames = c->elem[11];
	for (p = c->classnames; *p; p += strlen(p) + 1) c->nclass++;
	for (p = c->mapnames; *p; p += strlen(p) + 1) c->nmap++;
	if (72 + c->nclass + c->nmap > c->n) return -1;
	for (i = 0, p = c->classnames; i < c->nclass; i++, p += strlen(p) + 1)
		for (j = 0; j < W_NSTD; j++)
			if (!strcmp(p, std[j])) c->wclass[j] = (const uint32_t *)(const void *)c->elem[72 + i];
	for (i = 0, p = c->mapnames; i < c->nmap; i++, p += strlen(p) + 1) {
		if (!strcmp(p, "toupper")) c->wtoupper = (const uint32_t *)(const void *)c->elem[72 + c->nclass + i];
		if (!strcmp(p, "tolower")) c->wtolower = (const uint32_t *)(const void *)c->elem[72 + c->nclass + i];
	}
	for (j = 0; j < W_NSTD; j++) if (!c->wclass[j]) return -1;
	if (!c->wtoupper || !c->wtolower) return -1;
	c->wwidth = (const uint32_t *)(const void *)c->elem[12];
	c->ctype_b = (const uint16_t *)(const void *)c->elem[0];
	c->toupper_b = (const int32_t *)(const void *)c->elem[1];
	c->tolower_b = (const int32_t *)(const void *)c->elem[3];
	if (isutf8(cs)) { c->cs = CS_UTF8; return 0; }
	if (c->mbmax != 1) {
		/* EUC-JP, SHIFT_JIS, GB18030 ...: the codecs of iconv */
		if ((c->mbkind = __mbcs_kind(cs)) < 0) return -1;
		c->cs = CS_MB;
		return 0;
	}
	if (ieq(cs, "ANSI_X3.4-1968") || ieq(cs, "ASCII") || ieq(cs, "US-ASCII")) { c->cs = CS_C; return 0; }
	if (!(c->charset = __charset_find(cs))) {
		/* a codeset of its own (a charmap given to localedef): its bytes
		 * are converted as in the POSIX locale (glibc: as in C, too) */
		c->cs = CS_C;
		return 0;
	}
	c->cs = CS_8BIT;
	return 0;
}

/* LC_COLLATE: the tables of glibc's format (elements 0-5 and 9-12) */
static int collate_setup(struct __lcat *c)
{
	uint32_t nr;
	if (c->n < 13) return -1;
#define SZ(i) ((size_t)(c->elem[(i) + 1] - c->elem[i]))
	memcpy(&nr, c->elem[0], 4);
	if (nr > 255) return -1;
	c->nrules = (int)nr;
	if (!nr) return 0;
	if (SZ(1) < nr || SZ(2) < 256 * 4 || ((uintptr_t)c->elem[2] | (uintptr_t)c->elem[5] |
	    (uintptr_t)c->elem[9] | (uintptr_t)c->elem[10] | (uintptr_t)c->elem[11] |
	    (uintptr_t)c->elem[12]) & 3)
		return -1;
	c->rulesets = (const unsigned char *)c->elem[1];
	c->nrulesets = SZ(1);
	c->tablemb = (const int32_t *)(const void *)c->elem[2];
	c->weightmb = (const unsigned char *)c->elem[3];
	c->nweightmb = SZ(3);
	c->extramb = (const unsigned char *)c->elem[4];
	c->nextramb = SZ(4);
	c->indirectmb = (const int32_t *)(const void *)c->elem[5];
	c->nindirectmb = SZ(5) / 4;
	c->tablewc = (const uint32_t *)(const void *)c->elem[9];
	c->weightwc = (const int32_t *)(const void *)c->elem[10];
	c->nweightwc = SZ(10) / 4;
	c->extrawc = (const int32_t *)(const void *)c->elem[11];
	c->nextrawc = SZ(11) / 4;
	c->indirectwc = (const int32_t *)(const void *)c->elem[12];
	c->nindirectwc = SZ(12) / 4;
	if (c->n > 18 && SZ(16) >= 256 && !((uintptr_t)c->elem[17] & 3)) {
		c->collseqmb = (const unsigned char *)c->elem[16];
		c->collseqwc = (const uint32_t *)(const void *)c->elem[17];
	}
#undef SZ
	return 0;
}

static struct __lcat *load(int cat, const char *dir, const char *name)
{
	char path[LOCNAME_MAX * 2 + 64];
	size_t size, dl = strlen(dir), nl = strlen(name), cl = strlen(catnames[cat]), i;
	unsigned char *m;
	uint32_t magic, n, *offs;
	struct __lcat *c;
	const char **elem;
	static const uint32_t base[3] = { 0x20090720, 0x20051014, 0x20031115 };

	if (dl + nl + cl + 3 > sizeof path) return 0;
	memcpy(path, dir, dl);
	path[dl] = '/';
	memcpy(path + dl + 1, name, nl);
	path[dl + 1 + nl] = '/';
	memcpy(path + dl + 2 + nl, catnames[cat], cl + 1);
	if (!(m = mapfile(path, &size))) return 0;
	memcpy(&magic, m, 4);
	memcpy(&n, m + 4, 4);
	if (magic != ((cat == LC_CTYPE ? base[0] : cat == LC_COLLATE ? base[1] : base[2]) ^ (uint32_t)cat) ||
	    n > (size - 8) / 4 || n > 100000)
		goto bad;
	offs = (uint32_t *)(void *)(m + 8);
	for (i = 0; i < n; i++) if (offs[i] > size) goto bad;
	if (!(c = calloc(1, sizeof *c + n * sizeof *elem))) goto bad;
	elem = (const char **)(void *)(c + 1);
	for (i = 0; i < n; i++) elem[i] = (const char *)m + offs[i];
	c->cat = cat;
	c->n = n;
	c->elem = elem;
	c->cs = CS_C;
	c->mbmax = 1;
	if (cat == LC_CTYPE && ctype_setup(c)) { free(c); goto bad; }
	if (cat == LC_COLLATE && collate_setup(c)) { free(c); goto bad; }
	return c;
bad:
	__syscall2(SYS_munmap, m, size);
	return 0;
}

static struct __lcat *find(int cat, const char *name)
{
	char norm[LOCNAME_MAX + 8];
	const char *lp = __libc.secure ? 0 : getenv("LOCPATH"), *e;
	struct __lcat *c = 0;
	int hasnorm = normalize(name, norm, sizeof norm);
	if (strchr(name, '/')) {
		/* a directory named outright */
		if (__libc.secure) return 0;
		return load(cat, ".", name);
	}
	for (; lp && *lp && !c; lp = *e ? e + 1 : e) {
		char dir[512];
		e = strchr(lp, ':');
		if (!e) e = lp + strlen(lp);
		if (e == lp || (size_t)(e - lp) >= sizeof dir) continue;
		memcpy(dir, lp, (size_t)(e - lp));
		dir[e - lp] = 0;
		if (!(c = load(cat, dir, name)) && hasnorm) c = load(cat, dir, norm);
	}
	if (!c && !(c = load(cat, "/usr/lib/locale", name)) && hasnorm) c = load(cat, "/usr/lib/locale", norm);
	return c;
}

/* built-in data under another name: cached copies */
static const struct __lcat *named(int cat, const char *name, size_t l)
{
	struct __lcat *a;
	for (a = cache; a; a = a->next)
		if (a->cat == cat && !strcmp(a->name, name)) return a;
	if (!(a = malloc(sizeof *a))) return 0;
	*a = cat == LC_CTYPE ? __c_utf8_ctype : __c_cat[cat];
	memcpy(a->name, name, l + 1);
	a->next = cache;
	cache = a;
	return a;
}

/* the codeset part of a name ("en_US.UTF-8@euro" -> "UTF-8") is UTF-8 */
static int utf8name(const char *name)
{
	const char *dot = strchr(name, '.'), *at;
	char cs[32];
	size_t k;
	if (!dot) return 0;
	at = strchr(dot, '@');
	k = at ? (size_t)(at - dot - 1) : strlen(dot + 1);
	if (k >= sizeof cs) return 0;
	memcpy(cs, dot + 1, k);
	cs[k] = 0;
	return isutf8(cs);
}

const struct __lcat *__get_lcat(int cat, const char *name)
{
	const struct __lcat *c;
	struct __lcat *f;
	size_t l = strlen(name);
	if (!strcmp(name, "C") || !strcmp(name, "POSIX")) return &__c_cat[cat];
	if (!l || l > LOCNAME_MAX || strchr(name, ';') || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
	LOCK(&lock);
	/* C.UTF-8 (C.utf8 ...): built in */
	if ((!strncmp(name, "C.", 2) || !strncmp(name, "POSIX.", 6)) && utf8name(name) && !strchr(name, '@')) {
		c = named(cat, name, l);
		UNLOCK(&lock);
		return c;
	}
	for (c = cache; c; c = c->next)
		if (c->cat == cat && !strcmp(c->name, name)) break;
	if (!c && (f = find(cat, name))) {
		memcpy(f->name, name, l + 1);
		f->next = cache;
		cache = f;
		c = f;
	}
	/* a UTF-8 locale without data is taken as C.UTF-8 under its name */
	if (!c && !strchr(name, '/') && utf8name(name)) c = named(cat, name, l);
	UNLOCK(&lock);
	return c;
}

/* ---- 3-level tables ---- */

static const uint32_t *level(const uint32_t *t, uint32_t wc, uint32_t s5)
{
	uint32_t i1 = wc >> t[0], l1, l2;
	if (i1 >= t[1]) return 0;
	if (!(l1 = t[5 + i1])) return 0;
	if (!(l2 = *(const uint32_t *)(const void *)((const char *)t + l1 + 4 * ((wc >> t[2]) & t[3])))) return 0;
	(void)s5;
	return (const uint32_t *)(const void *)((const char *)t + l2);
}

int __tab_bit(const uint32_t *t, uint32_t wc)
{
	const uint32_t *l3 = level(t, wc, 5);
	return l3 ? (int)((l3[(wc >> 5) & t[4]] >> (wc & 31)) & 1) : 0;
}

/* a y >= wc with __tab_bit(t, x) == __tab_bit(t, wc) for x in [wc, y]:
 * the end of the empty block, or of the run in the bitmap word */
uint32_t __tab_bit_run(const uint32_t *t, uint32_t wc)
{
	uint32_t i1 = wc >> t[0], l1, l2, w, v;
	if (i1 >= t[1]) return 0x10ffff;
	if (!(l1 = t[5 + i1])) return wc | ((1u << t[0]) - 1);
	if (!(l2 = *(const uint32_t *)(const void *)((const char *)t + l1 + 4 * ((wc >> t[2]) & t[3]))))
		return wc | ((1u << t[2]) - 1);
	w = ((const uint32_t *)(const void *)((const char *)t + l2))[(wc >> 5) & t[4]];
	v = w >> (wc & 31) & 1;
	while ((wc & 31) != 31 && (w >> ((wc + 1) & 31) & 1) == v) wc++;
	return wc;
}

/* a y >= wc such that wc..y are all in one empty block of table t (all
 * map to 0), or wc */
uint32_t __tab_run(const uint32_t *t, uint32_t wc)
{
	uint32_t i1 = wc >> t[0], l1;
	if (i1 >= t[1]) return 0x10ffff > wc ? 0x10ffff : wc;
	if (!(l1 = t[5 + i1])) return wc | ((1u << t[0]) - 1);
	if (!*(const uint32_t *)(const void *)((const char *)t + l1 + 4 * ((wc >> t[2]) & t[3])))
		return wc | ((1u << t[2]) - 1);
	return wc;
}

int32_t __tab_map(const uint32_t *t, uint32_t wc)
{
	const uint32_t *l3 = level(t, wc, 0);
	return l3 ? (int32_t)l3[wc & t[4]] : 0;
}

int __tab_byte(const uint32_t *t, uint32_t wc)
{
	const uint32_t *l3 = level(t, wc, 0);
	return l3 ? ((const unsigned char *)l3)[wc & t[4]] : 0xff;
}

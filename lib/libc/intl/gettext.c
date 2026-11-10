/* The gettext family: message objects in the .mo format of msgfmt (magic
 * 0x950412de; tables of the original and translated strings, the
 * originals sorted). Plural forms by the Plural-Forms expression of the
 * header; translations converted to the output codeset with iconv. */
#include <libintl.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include "intl_impl.h"
#include "locale_impl.h"
#include "lock.h"
#include "libc.h"

#define DEFDIR "/usr/share/locale"

struct binding {
	struct binding *next;
	char *dir, *codeset;
	char domain[];
};

struct mo {
	struct mo *next;
	char *key;                  /* dir/locale/category/domain.mo */
	unsigned char *map;         /* 0: no file */
	size_t size;
	uint32_t n, otab, ttab;
	int swap;
	const char *plural;         /* "plural=" expression, or 0 */
	unsigned long nplurals;
	char *charset;              /* of the file */
	/* converted translations, per output codeset (the last used) */
	char *outcs;
	char **conv;
	uint32_t *convlen;
};

static volatile int lock;
static struct binding *bindings;
static struct mo *mos;
static char domain[TEXTDOMAIN_MAX + 1] = "messages";

static uint32_t u32(const struct mo *m, uint32_t off)
{
	uint32_t v;
	memcpy(&v, m->map + off, 4);
	return m->swap ? (v >> 24 | (v >> 8 & 0xff00) | (v << 8 & 0xff0000) | v << 24) : v;
}

/* the string at table tab, entry i: its text (0 if malformed) and length */
static const char *str(const struct mo *m, uint32_t tab, uint32_t i, uint32_t *len)
{
	uint32_t l = u32(m, tab + 8 * i), o = u32(m, tab + 8 * i + 4);
	if (o >= m->size || l >= m->size - o || m->map[o + l]) return 0;
	*len = l;
	return (const char *)m->map + o;
}

/* ---- the plural expression: C syntax, the variable n ---- */

struct ev { const char *s; unsigned long n; int err; };

static void ws(struct ev *e) { while (*e->s == ' ' || *e->s == '\t' || *e->s == '\n') e->s++; }
static unsigned long cond(struct ev *e);

static unsigned long prim(struct ev *e)
{
	unsigned long v = 0;
	ws(e);
	if (*e->s == '(') {
		e->s++;
		v = cond(e);
		ws(e);
		if (*e->s == ')') e->s++; else e->err = 1;
		return v;
	}
	if (*e->s == '!') { e->s++; return !prim(e); }
	if (*e->s == 'n') { e->s++; return e->n; }
	if (*e->s < '0' || *e->s > '9') { e->err = 1; return 0; }
	while (*e->s >= '0' && *e->s <= '9') v = v * 10 + (unsigned long)(*e->s++ - '0');
	return v;
}

/* binary operators by precedence: 0 || 1 && 2 == != 3 < > <= >= 4 + - 5 * / % */
static int op(struct ev *e, int lvl, int *len)
{
	const char *s = e->s;
	static const char *const ops[6][4] = { { "||" }, { "&&" }, { "==", "!=" }, { "<=", ">=", "<", ">" },
		{ "+", "-" }, { "*", "/", "%" } };
	int i;
	for (i = 0; i < 4 && ops[lvl][i]; i++) {
		size_t l = strlen(ops[lvl][i]);
		if (!strncmp(s, ops[lvl][i], l)) { *len = (int)l; return i; }
	}
	return -1;
}

static unsigned long bin(struct ev *e, int lvl)
{
	unsigned long a, b;
	int o, l;
	if (lvl == 6) return prim(e);
	a = bin(e, lvl + 1);
	for (;;) {
		ws(e);
		if ((o = op(e, lvl, &l)) < 0) return a;
		e->s += l;
		b = bin(e, lvl + 1);
		switch (lvl) {
		case 0: a = a || b; break;
		case 1: a = a && b; break;
		case 2: a = o ? a != b : a == b; break;
		case 3: a = o == 0 ? a <= b : o == 1 ? a >= b : o == 2 ? a < b : a > b; break;
		case 4: a = o ? a - b : a + b; break;
		case 5:
			if (o && !b) { e->err = 1; return 0; }
			a = o == 0 ? a * b : o == 1 ? a / b : a % b;
			break;
		}
	}
}

static unsigned long cond(struct ev *e)
{
	unsigned long c = bin(e, 0), a, b;
	ws(e);
	if (*e->s != '?') return c;
	e->s++;
	a = cond(e);
	ws(e);
	if (*e->s != ':') { e->err = 1; return 0; }
	e->s++;
	b = cond(e);
	return c ? a : b;
}

static unsigned long plural(const struct mo *m, unsigned long n)
{
	struct ev e;
	unsigned long v;
	if (!m->plural) return n != 1;
	e.s = m->plural; e.n = n; e.err = 0;
	v = cond(&e);
	if (e.err || v >= m->nplurals) return n != 1 && m->nplurals > 1;
	return v;
}

/* ---- message objects ---- */

static void header(struct mo *m)
{
	uint32_t l, i;
	const char *h = 0, *p, *e;
	for (i = 0; i < m->n && !h; i++) {
		const char *o = str(m, m->otab, i, &l);
		if (o && !l) h = str(m, m->ttab, i, &l);
	}
	if (!h) return;
	if ((p = strstr(h, "charset="))) {
		p += 8;
		for (e = p; *e && *e != '\n' && *e != ' ' && *e != ';'; e++);
		if ((m->charset = malloc((size_t)(e - p) + 1))) {
			memcpy(m->charset, p, (size_t)(e - p));
			m->charset[e - p] = 0;
		}
	}
	if ((p = strstr(h, "Plural-Forms:"))) {
		const char *np = strstr(p, "nplurals="), *pl = strstr(p, "plural=");
		const char *eol = strchr(p, '\n');
		if (np && (!eol || np < eol)) m->nplurals = strtoul(np + 9, 0, 10);
		/* "plural=" not within "nplurals=" */
		while (pl && pl > p && pl[-1] == 'n') pl = strstr(pl + 1, "plural=");
		if (pl && (!eol || pl < eol)) m->plural = pl + 7;
	}
}

static struct mo *load(const char *path)
{
	struct mo *m;
	size_t size, kl = strlen(path);
	unsigned char *map;
	uint32_t magic;
	for (m = mos; m; m = m->next) if (!strcmp(m->key, path)) return m->map ? m : 0;
	if (!(m = calloc(1, sizeof *m + kl + 1))) return 0;
	m->key = (char *)(m + 1);
	memcpy(m->key, path, kl + 1);
	m->next = mos;
	mos = m;
	if (!(map = __map_file(path, &size))) return 0;
	if (size < 28) goto bad;
	memcpy(&magic, map, 4);
	if (magic == 0x950412de) m->swap = 0;
	else if (magic == 0xde120495) m->swap = 1;
	else goto bad;
	m->map = map;
	m->size = size;
	m->n = u32(m, 8);
	m->otab = u32(m, 12);
	m->ttab = u32(m, 16);
	if (m->otab > size || m->ttab > size || m->n > (size - m->otab) / 8 || m->n > (size - m->ttab) / 8) {
		m->map = 0;
		goto bad;
	}
	header(m);
	return m;
bad:
	munmap(map, size);
	return 0;
}

/* the translation of msgid (and its plural form), or 0 */
static const char *find(struct mo *m, const char *msgid, const char *plu, unsigned long n, const char *outcs)
{
	uint32_t lo = 0, hi = m->n, l, i;
	const char *t;
	while (lo < hi) {
		uint32_t mid = (lo + hi) / 2;
		const char *o = str(m, m->otab, mid, &l);
		int c;
		if (!o) return 0;
		c = strcmp(msgid, o);
		if (!c) {
			i = mid;
			if (!(t = str(m, m->ttab, i, &l))) return 0;
			/* converted, if the codesets differ */
			if (m->charset && outcs && *outcs && strcasecmp(m->charset, outcs)) {
				if (!m->outcs || strcmp(m->outcs, outcs)) {
					size_t k;
					if (m->conv) for (k = 0; k < m->n; k++) free(m->conv[k]);
					free(m->conv);
					free(m->convlen);
					free(m->outcs);
					m->conv = calloc(m->n, sizeof *m->conv);
					m->convlen = calloc(m->n, sizeof *m->convlen);
					m->outcs = strdup(outcs);
					if (!m->conv || !m->convlen || !m->outcs) return 0;
				}
				if (!m->conv[i]) {
					iconv_t cd = iconv_open(outcs, m->charset);
					char *ip = (char *)t, *buf, *op;
					size_t il = l + 1, ol = 4 * (l + 1) + 16;
					if (cd == (iconv_t)-1) return 0;
					if (!(buf = malloc(ol))) { iconv_close(cd); return 0; }
					op = buf;
					if (iconv(cd, &ip, &il, &op, &ol) == (size_t)-1) { iconv_close(cd); free(buf); return 0; }
					iconv(cd, 0, 0, &op, &ol);
					iconv_close(cd);
					m->conv[i] = buf;
					/* (the null bytes between plural forms are converted too) */
					m->convlen[i] = (uint32_t)(op - buf - 1);
				}
				t = m->conv[i];
				l = m->convlen[i];
			}
			if (plu) {
				unsigned long k = plural(m, n);
				const char *f = t, *end = t + l;
				for (; k && f < end; k--) f += strlen(f) + 1;
				if (f >= end && k == 0 && f != t) return 0;
				return f;
			}
			return t;
		}
		if (c < 0) hi = mid; else lo = mid + 1;
	}
	return 0;
}

struct look {
	const char *dir, *cat, *dom, *msgid, *plu, *outcs;
	unsigned long n;
	const char *r;
};

static int try(const char *loc, struct look *k)
{
	char path[PATH_MAX];
	struct mo *m;
	size_t need = strlen(k->dir) + strlen(loc) + strlen(k->cat) + strlen(k->dom) + 6;
	if (need > sizeof path) return 0;
	strcpy(path, k->dir);
	strcat(path, "/"); strcat(path, loc);
	strcat(path, "/"); strcat(path, k->cat);
	strcat(path, "/"); strcat(path, k->dom); strcat(path, ".mo");
	if (!(m = load(path))) return 0;
	k->r = find(m, k->msgid, k->plu, k->n, k->outcs);
	return 1;   /* found the object: used (even if it lacks the message) */
}

/* loc, then without .codeset, without _territory, without @modifier */
static int trylocale(const char *loc, struct look *k)
{
	char buf[256];
	const char *at = strchr(loc, '@'), *dot = strchr(loc, '.'), *us = strchr(loc, '_');
	size_t l = strlen(loc);
	if (l >= sizeof buf) return 0;
	if (try(loc, k)) return 1;
	if (dot) {
		/* language_territory@modifier */
		size_t a = (size_t)(dot - loc);
		memcpy(buf, loc, a);
		strcpy(buf + a, at ? at : "");
		if (try(buf, k)) return 1;
	}
	if (us) {
		size_t a = (size_t)(us - loc);
		memcpy(buf, loc, a);
		strcpy(buf + a, at ? at : "");
		if (try(buf, k)) return 1;
		buf[a] = 0;
		if (at && try(buf, k)) return 1;
	} else if (at) {
		memcpy(buf, loc, (size_t)(at - loc));
		buf[at - loc] = 0;
		if (try(buf, k)) return 1;
	}
	return 0;
}

static int trynls(const char *path, void *arg)
{
	struct look *k = arg;
	struct mo *m = load(path);
	if (!m) return 0;
	k->r = find(m, k->msgid, k->plu, k->n, k->outcs);
	return 1;
}

static const char *const catnames[6] = {
	"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES",
};

char *dcngettext_l(const char *dom, const char *msgid, const char *plu, unsigned long n, int cat, locale_t l)
{
	const char *loc, *lang, *nls, *r = 0;
	struct binding *b;
	struct look k;
	int e = errno;
	if (l == LC_GLOBAL_LOCALE) l = &__global_locale;
	if ((unsigned)cat >= LC_ALL || !msgid) goto none;
	loc = l->cat[cat]->name;
	if (!strcmp(loc, "C") || !strcmp(loc, "POSIX")) goto none;
	LOCK(&lock);
	if (!dom) dom = domain;
	for (b = bindings; b && strcmp(b->domain, dom); b = b->next);
	k.dir = b && b->dir ? b->dir : DEFDIR;
	k.cat = catnames[cat];
	k.dom = dom;
	k.msgid = msgid;
	k.plu = plu;
	k.n = n;
	k.outcs = b && b->codeset ? b->codeset : __lc_str(l->cat[LC_CTYPE], 14);
	k.r = 0;
	nls = __libc.secure ? 0 : getenv("NLSPATH");
	if (nls && *nls && __nlspath(nls, dom, loc, trynls, &k)) r = k.r;
	if (!r && (lang = getenv("LANGUAGE")) && *lang) {
		while (*lang && !r) {
			char one[256];
			const char *c = strchr(lang, ':');
			size_t len = c ? (size_t)(c - lang) : strlen(lang);
			if (len && len < sizeof one) {
				memcpy(one, lang, len);
				one[len] = 0;
				k.r = 0;
				if (trylocale(one, &k)) r = k.r;
			}
			lang += len + (c != 0);
		}
	}
	if (!r) { k.r = 0; if (trylocale(loc, &k)) r = k.r; }
	UNLOCK(&lock);
	if (r) { errno = e; return (char *)r; }
none:
	errno = e;
	return (char *)(plu && n != 1 ? plu : msgid);
}

char *dcngettext(const char *d, const char *m, const char *p, unsigned long n, int c)
{ return dcngettext_l(d, m, p, n, c, CURRENT_LOCALE); }
char *dcgettext_l(const char *d, const char *m, int c, locale_t l) { return dcngettext_l(d, m, 0, 1, c, l); }
char *dcgettext(const char *d, const char *m, int c) { return dcngettext_l(d, m, 0, 1, c, CURRENT_LOCALE); }
char *dngettext_l(const char *d, const char *m, const char *p, unsigned long n, locale_t l)
{ return dcngettext_l(d, m, p, n, LC_MESSAGES, l); }
char *dngettext(const char *d, const char *m, const char *p, unsigned long n)
{ return dcngettext_l(d, m, p, n, LC_MESSAGES, CURRENT_LOCALE); }
char *dgettext_l(const char *d, const char *m, locale_t l) { return dcngettext_l(d, m, 0, 1, LC_MESSAGES, l); }
char *dgettext(const char *d, const char *m) { return dcngettext_l(d, m, 0, 1, LC_MESSAGES, CURRENT_LOCALE); }
char *ngettext_l(const char *m, const char *p, unsigned long n, locale_t l) { return dcngettext_l(0, m, p, n, LC_MESSAGES, l); }
char *ngettext(const char *m, const char *p, unsigned long n) { return dcngettext_l(0, m, p, n, LC_MESSAGES, CURRENT_LOCALE); }
char *gettext_l(const char *m, locale_t l) { return dcngettext_l(0, m, 0, 1, LC_MESSAGES, l); }
char *gettext(const char *m) { return dcngettext_l(0, m, 0, 1, LC_MESSAGES, CURRENT_LOCALE); }

char *textdomain(const char *d)
{
	static char r[TEXTDOMAIN_MAX + 1];
	LOCK(&lock);
	if (d) {
		size_t l = strlen(d);
		if (l > TEXTDOMAIN_MAX) { UNLOCK(&lock); errno = EINVAL; return 0; }
		if (!l) strcpy(domain, "messages");
		else memcpy(domain, d, l + 1);
	}
	strcpy(r, domain);
	UNLOCK(&lock);
	return r;
}

static struct binding *bind(const char *dom, const char *dir, const char *cs, int setdir)
{
	struct binding *b;
	for (b = bindings; b && strcmp(b->domain, dom); b = b->next);
	if (!b) {
		size_t l = strlen(dom);
		if (!(b = calloc(1, sizeof *b + l + 1))) return 0;
		memcpy(b->domain, dom, l + 1);
		b->next = bindings;
		bindings = b;
	}
	if (setdir && dir) {
		char *d = strdup(dir);
		size_t l;
		if (!d) return 0;
		for (l = strlen(d); l > 1 && d[l - 1] == '/'; l--) d[l - 1] = 0;
		free(b->dir);
		b->dir = d;
	}
	if (!setdir && cs) {
		char *c = strdup(cs);
		if (!c) return 0;
		free(b->codeset);
		b->codeset = c;
	}
	return b;
}

char *bindtextdomain(const char *dom, const char *dir)
{
	struct binding *b;
	char *r;
	if (!dom || !*dom)
		return 0;               /* no change, errno kept (POSIX) */
	if (strlen(dom) > TEXTDOMAIN_MAX) { errno = EINVAL; return 0; }
	LOCK(&lock);
	b = bind(dom, dir, 0, 1);
	r = b ? (b->dir ? b->dir : DEFDIR) : 0;
	UNLOCK(&lock);
	return r;
}

char *bind_textdomain_codeset(const char *dom, const char *cs)
{
	struct binding *b;
	char *r;
	if (!dom || !*dom)
		return 0;               /* no change, errno kept (POSIX) */
	if (strlen(dom) > TEXTDOMAIN_MAX) { errno = EINVAL; return 0; }
	LOCK(&lock);
	b = bind(dom, 0, cs, 0);
	r = b ? b->codeset : 0;
	UNLOCK(&lock);
	return r;
}

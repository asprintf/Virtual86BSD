/* Internal: locales.
 *
 * A locale object holds one pointer per category to the data of that
 * category: the built-in C (POSIX) data, the built-in C.UTF-8 character
 * data, or a category file in the format of the GNU C library (which the
 * localedef of the POSIX utilities writes). Category data, once loaded,
 * is never freed, so locale objects can share it freely.
 *
 * The data of a category is a list of elements indexed as in those files:
 * the low 16 bits of an nl_item. */
#ifndef _INTERNAL_LOCALE_IMPL_H
#define _INTERNAL_LOCALE_IMPL_H
#include <locale.h>
#include <stdint.h>
#include <stddef.h>
#include "pthread_impl.h"

#define LOCNAME_MAX 255

enum { CS_C, CS_UTF8, CS_8BIT, CS_MB };   /* CS_MB: EUC-JP, GB18030 ... (mbkind) */

/* the twelve standard classes, in the order of wctype_t values 1..12 */
enum { W_ALNUM, W_ALPHA, W_BLANK, W_CNTRL, W_DIGIT, W_GRAPH, W_LOWER, W_PRINT, W_PUNCT,
       W_SPACE, W_UPPER, W_XDIGIT, W_NSTD };

struct __charset;

struct __lcat {
	struct __lcat *next;        /* the cache of loaded categories */
	int cat;
	char name[LOCNAME_MAX + 1];
	unsigned n;                 /* elements */
	const char *const *elem;
	/* LC_CTYPE */
	int cs;                     /* CS_C, CS_UTF8 (built-in tables if no file), CS_8BIT */
	int mbmax;
	const uint16_t *ctype_b;    /* the classes of bytes -128..255, glibc's bits */
	const int32_t *toupper_b, *tolower_b;
	const uint32_t *wclass[W_NSTD];   /* 3-level tables; null: built-in */
	const uint32_t *wtoupper, *wtolower, *wwidth;
	const char *classnames, *mapnames;
	unsigned nclass, nmap;
	const struct __charset *charset;  /* CS_8BIT: the bytes 0x80-0xff */
	int mbkind;                 /* CS_MB: the codec (iconv_impl.h) */
	/* LC_COLLATE (glibc's tables; nrules 0: code point order) */
	int nrules;
	const unsigned char *rulesets;     /* [rule * nrules + level]: forward 1, backward 2, position 4 */
	const int32_t *tablemb;            /* 256 entries */
	const unsigned char *weightmb, *extramb;
	const int32_t *indirectmb;
	const uint32_t *tablewc;           /* 3-level */
	const int32_t *weightwc, *extrawc, *indirectwc;
	size_t nrulesets, nweightmb, nextramb, nindirectmb, nweightwc, nextrawc, nindirectwc;
	const unsigned char *collseqmb;    /* 256: the collation sequence of bytes */
	const uint32_t *collseqwc;         /* 3-level: of characters */
	const unsigned char *symbextra;    /* multi-character collating elements: [len name][len bytes]
	                                      pad, seqmb, nwcs, wcs[nwcs], seqwc (glibc's SYMB_EXTRAMB) */
	size_t nsymbextra;
};

struct __locale_struct {
	const struct __lcat *cat[6];
};

extern struct __locale_struct __global_locale;
extern const struct __lcat __c_cat[6], __c_utf8_ctype;
extern const struct __locale_struct __c_locale_obj;

/* the locale of the calling thread */
static inline locale_t __current_locale(void)
{
	struct __pthread *s = __pthread_self();
	return s->locale ? (locale_t)s->locale : &__global_locale;
}
#define CURRENT_LOCALE (__current_locale())
#define CURRENT_CTYPE (CURRENT_LOCALE->cat[LC_CTYPE])
#define CURRENT_UTF8 (CURRENT_CTYPE->cs == CS_UTF8)

/* the category data for a name ("" not allowed here), or null */
const struct __lcat *__get_lcat(int cat, const char *name);
/* element idx of a category as a string, "" if absent */
const char *__lc_str(const struct __lcat *, unsigned idx);

/* 3-level tables */
int __tab_bit(const uint32_t *t, uint32_t wc);
uint32_t __tab_bit_run(const uint32_t *t, uint32_t wc);
int32_t __tab_map(const uint32_t *t, uint32_t wc);
uint32_t __tab_run(const uint32_t *t, uint32_t wc);

/* collation by LC_COLLATE c (wide: wchar_t strings) */
int __coll_cmp(const struct __lcat *c, const void *a, const void *b, int wide);
size_t __coll_xfrm(const struct __lcat *c, void *d, const void *s, size_t n, int wide);
/* x and y of one equivalence class (first-level weights) */
int __coll_equiv(const struct __lcat *c, int wide, uint32_t x, uint32_t y);
int __coll_valid(const struct __lcat *c, int wide, uint32_t x);
/* the collation sequence of a character (bracket ranges) */
uint32_t __coll_seq(const struct __lcat *c, int wide, uint32_t x);
int __coll_seq_runs(const struct __lcat *c, uint32_t sc, uint32_t sd,
                    int (*f)(void *, uint32_t, uint32_t), void *arg);
/* Multi-character collating elements ("ch"), and the same questions for
 * a collating element given as a string of units (wide: code points;
 * else bytes) as for one character. */
struct __coll_me {
	const unsigned char *mbs;          /* the bytes (the locale's codeset) */
	const unsigned char *wcs;          /* nwcs code points, unaligned */
	size_t nmbs, nwcs;
	uint32_t seqmb, seqwc;             /* its collation sequence (bytes, wide) */
};
/* the next element after *pos (0 to start); 0 at the end */
int __coll_me_next(const struct __lcat *c, size_t *pos, struct __coll_me *e);
/* the units of e (wide: code points) into u (at least 256); their number */
size_t __coll_me_units(const struct __coll_me *e, int wide, uint32_t *u);
/* x[0..n) is a collating element of the locale (one character: as __coll_valid) */
int __coll_valids(const struct __lcat *c, int wide, const uint32_t *x, size_t n);
int __coll_equivs(const struct __lcat *c, int wide, const uint32_t *x, size_t nx,
                  const uint32_t *y, size_t ny);
/* its collation sequence: __coll_seq(c, wide, *x) for one unit, else on the
   wide scale (elements have none on the byte scale); ~0 if none */
uint32_t __coll_seqs(const struct __lcat *c, int wide, const uint32_t *x, size_t n);
#define COLL_OF(l) (((l) == LC_GLOBAL_LOCALE ? &__global_locale : (l))->cat[LC_COLLATE])
int __tab_byte(const uint32_t *t, uint32_t wc);

/* the built-in Unicode data (C.UTF-8) */
int __uctype_class(int cls, uint32_t wc);
uint32_t __uctype_run(int cls, uint32_t wc);
uint32_t __uctype_toupper(uint32_t wc);
uint32_t __uctype_tolower(uint32_t wc);
int __uctype_width(uint32_t wc);       /* for printable characters */

/* wide character classes and mappings in a locale's LC_CTYPE: t is a
 * wctype_t / wctrans_t value */
int __iswctype_l(unsigned wc, unsigned long t, const struct __lcat *);
unsigned __iswctype_run(unsigned wc, unsigned long t, const struct __lcat *);
unsigned __towctrans_l(unsigned wc, unsigned long t, const struct __lcat *);
unsigned long __wctype_l(const char *, const struct __lcat *);
unsigned long __wctrans_l(const char *, const struct __lcat *);

/* single-byte codesets */
struct __charset {
	const char *name;           /* canonical name */
	const uint16_t *hi;         /* 128 entries: the characters of 0x80-0xff, 0 if none */
};
const struct __charset *__charset_find(const char *name);
int __charset_from_wc(const struct __charset *, uint32_t wc);   /* byte, or -1 */

#endif

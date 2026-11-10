/* The _l forms: the classes of a given locale. */
#include <ctype.h>
#include <locale.h>
#include "locale_impl.h"

enum { B_UPPER = 0x100, B_LOWER = 0x200, B_ALPHA = 0x400, B_DIGIT = 0x800, B_XDIGIT = 0x1000,
       B_SPACE = 0x2000, B_PRINT = 0x4000, B_GRAPH = 0x8000, B_BLANK = 1, B_CNTRL = 2, B_PUNCT = 4,
       B_ALNUM = 8 };

static const struct __lcat *ct(locale_t l)
{
	return (l == LC_GLOBAL_LOCALE ? &__global_locale : l)->cat[LC_CTYPE];
}

static int cls(int c, int bit, int ascii, locale_t l)
{
	const struct __lcat *t;
	if ((unsigned)c < 0x80) return ascii;
	if ((unsigned)c - 0x80 >= 0x80 || !(t = ct(l))->ctype_b || t->cs == CS_UTF8) return 0;
	return (t->ctype_b[c + 128] & bit) != 0;
}

int isalnum_l(int c, locale_t l) { return cls(c, B_ALNUM, ((unsigned)c | 32) - 'a' < 26 || (unsigned)c - '0' < 10, l); }
int isalpha_l(int c, locale_t l) { return cls(c, B_ALPHA, ((unsigned)c | 32) - 'a' < 26, l); }
int isblank_l(int c, locale_t l) { return cls(c, B_BLANK, c == ' ' || c == '\t', l); }
int iscntrl_l(int c, locale_t l) { return cls(c, B_CNTRL, (unsigned)c < 0x20 || c == 0x7f, l); }
int isdigit_l(int c, locale_t l) { (void)l; return (unsigned)c - '0' < 10; }
int isgraph_l(int c, locale_t l) { return cls(c, B_GRAPH, (unsigned)c - 0x21 < 0x5e, l); }
int islower_l(int c, locale_t l) { return cls(c, B_LOWER, (unsigned)c - 'a' < 26, l); }
int isprint_l(int c, locale_t l) { return cls(c, B_PRINT, (unsigned)c - 0x20 < 0x5f, l); }
int ispunct_l(int c, locale_t l)
{
	return cls(c, B_PUNCT, (unsigned)c - 0x21 < 0x5e && !(((unsigned)c | 32) - 'a' < 26) && !((unsigned)c - '0' < 10), l);
}
int isspace_l(int c, locale_t l) { return cls(c, B_SPACE, c == ' ' || (unsigned)c - '\t' < 5, l); }
int isupper_l(int c, locale_t l) { return cls(c, B_UPPER, (unsigned)c - 'A' < 26, l); }
int isxdigit_l(int c, locale_t l) { (void)l; return (unsigned)c - '0' < 10 || ((unsigned)c | 32) - 'a' < 6; }

int tolower_l(int c, locale_t l)
{
	const struct __lcat *t;
	if ((unsigned)c >= 0x100) return c;
	if ((t = ct(l))->tolower_b) return t->tolower_b[c + 128];
	return (unsigned)c - 'A' < 26 ? c | 32 : c;
}

int toupper_l(int c, locale_t l)
{
	const struct __lcat *t;
	if ((unsigned)c >= 0x100) return c;
	if ((t = ct(l))->toupper_b) return t->toupper_b[c + 128];
	return (unsigned)c - 'a' < 26 ? c & 0x5f : c;
}

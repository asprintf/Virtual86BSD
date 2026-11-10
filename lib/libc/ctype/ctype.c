/* Character classification of bytes. The ASCII range is the same in
 * every locale this library supports; bytes 0x80-0xff belong to classes
 * only in single-byte locales loaded from files, whose tables (in the
 * GNU C library's format) say which. */
#include <ctype.h>
#include "locale_impl.h"

#undef isalnum
#undef isalpha
#undef isblank
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit

/* glibc's class bits, as its little-endian tables store them */
enum { B_UPPER = 0x100, B_LOWER = 0x200, B_ALPHA = 0x400, B_DIGIT = 0x800, B_XDIGIT = 0x1000,
       B_SPACE = 0x2000, B_PRINT = 0x4000, B_GRAPH = 0x8000, B_BLANK = 1, B_CNTRL = 2, B_PUNCT = 4,
       B_ALNUM = 8 };

static int hi(int c, int bit)
{
	const struct __lcat *ct;
	if ((unsigned)c - 0x80 >= 0x80) return 0;
	ct = CURRENT_CTYPE;
	/* the table of a locale file (a single-byte one, or one with a charmap
	 * of its own; in UTF-8 locales its bytes 0x80-0xff have no classes) */
	return ct->ctype_b && ct->cs != CS_UTF8 && (ct->ctype_b[c + 128] & bit);
}

int isalnum(int c) { return (unsigned)c < 0x80 ? ((unsigned)c | 32) - 'a' < 26 || (unsigned)c - '0' < 10 : hi(c, B_ALNUM); }
int isalpha(int c) { return (unsigned)c < 0x80 ? ((unsigned)c | 32) - 'a' < 26 : hi(c, B_ALPHA); }
int isblank(int c) { return (unsigned)c < 0x80 ? c == ' ' || c == '\t' : hi(c, B_BLANK); }
int iscntrl(int c) { return (unsigned)c < 0x80 ? (unsigned)c < 0x20 || c == 0x7f : hi(c, B_CNTRL); }
int isdigit(int c) { return (unsigned)c - '0' < 10; }
int isgraph(int c) { return (unsigned)c < 0x80 ? (unsigned)c - 0x21 < 0x5e : hi(c, B_GRAPH); }
int islower(int c) { return (unsigned)c < 0x80 ? (unsigned)c - 'a' < 26 : hi(c, B_LOWER); }
int isprint(int c) { return (unsigned)c < 0x80 ? (unsigned)c - 0x20 < 0x5f : hi(c, B_PRINT); }
int ispunct(int c) { return (unsigned)c < 0x80 ? (unsigned)c - 0x21 < 0x5e && !(((unsigned)c | 32) - 'a' < 26) && !((unsigned)c - '0' < 10) : hi(c, B_PUNCT); }
int isspace(int c) { return (unsigned)c < 0x80 ? c == ' ' || (unsigned)c - '\t' < 5 : hi(c, B_SPACE); }
int isupper(int c) { return (unsigned)c < 0x80 ? (unsigned)c - 'A' < 26 : hi(c, B_UPPER); }
int isxdigit(int c) { return (unsigned)c - '0' < 10 || ((unsigned)c | 32) - 'a' < 6; }

/* case mappings: the locale's tables if it has them (Turkish maps 'i'
 * otherwise), else ASCII */
int tolower(int c)
{
	const struct __lcat *ct;
	if ((unsigned)c >= 0x100) return c;
	if ((ct = CURRENT_CTYPE)->tolower_b) return ct->tolower_b[c + 128];
	return (unsigned)c - 'A' < 26 ? c | 32 : c;
}

int toupper(int c)
{
	const struct __lcat *ct;
	if ((unsigned)c >= 0x100) return c;
	if ((ct = CURRENT_CTYPE)->toupper_b) return ct->toupper_b[c + 128];
	return (unsigned)c - 'a' < 26 ? c & 0x5f : c;
}

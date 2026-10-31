/* The scanf engine: all conversions of C17 and POSIX.1-2024, with field
 * widths, assignment suppression (*), positional arguments (%n$), the
 * 'm' assignment-allocation modifier and the length modifiers
 * hh h l ll j z t L.  Numbers are read by __intscan and __floatscan. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include "stdio_impl.h"
#include "locale_impl.h"
#include "mb_impl.h"

#define SIZE_hh -2
#define SIZE_h  -1
#define SIZE_def 0
#define SIZE_l   1
#define SIZE_L   2
#define SIZE_ll  3

static void store_int(void *dest, int size, unsigned long long i)
{
	if (!dest) return;
	switch (size) {
	case SIZE_hh: *(char *)dest = (char)i; break;
	case SIZE_h:  *(short *)dest = (short)i; break;
	case SIZE_def: *(int *)dest = (int)i; break;
	case SIZE_l:  *(long *)dest = (long)i; break;
	case SIZE_ll: *(long long *)dest = (long long)i; break;
	}
}

static void *arg_n(va_list ap, unsigned n)
{
	void *p;
	unsigned i;
	va_list ap2;
	va_copy(ap2, ap);
	for (i = n; i > 1; i--) va_arg(ap2, void *);
	p = va_arg(ap2, void *);
	va_end(ap2);
	return p;
}


static int isspace_(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }

int __vfscanf(FILE *f, const char *fmt, va_list ap)
{
	int width, size, alloc = 0, base, t, invert, matches = 0;
	const unsigned char *p;
	unsigned char scanset[257];
	size_t i, k;
	off_t pos = 0;
	void *dest = 0;
	char *s = 0;
	wchar_t *wcs = 0;
	unsigned long long x;
	long double y;

	if (!f->rpos) __toread(f);
	if (!f->rpos) goto input_fail;

	for (p = (const unsigned char *)fmt; *p; p++) {
		alloc = 0;

		if (isspace_(*p)) {
			while (isspace_(p[1])) p++;
			shlim(f, 0);
			while (isspace_(shgetc(f)));
			shunget(f);
			pos += shcnt(f);
			continue;
		}
		if (*p != '%' || p[1] == '%') {
			int c;
			shlim(f, 0);
			if (*p == '%') {
				p++;
				while (isspace_((c = shgetc(f))));
			} else {
				c = shgetc(f);
			}
			if (c != *p) {
				shunget(f);
				if (c < 0) goto input_fail;
				goto match_fail;
			}
			pos += shcnt(f);
			continue;
		}

		p++;
		if (*p == '*') {
			dest = 0; p++;
		} else if ((unsigned)*p - '0' < 10 && p[1] == '$') {
			dest = arg_n(ap, (unsigned)(*p - '0')); p += 2;
		} else if ((unsigned)*p - '0' < 10 && (unsigned)p[1] - '0' < 10 && p[2] == '$') {
			dest = arg_n(ap, (unsigned)((p[0] - '0') * 10 + p[1] - '0')); p += 3;
		} else {
			dest = va_arg(ap, void *);
		}

		for (width = 0; (unsigned)*p - '0' < 10; p++) width = 10 * width + (*p - '0');

		if (*p == 'm') { wcs = 0; s = 0; alloc = !!dest; p++; }
		else alloc = 0;

		size = SIZE_def;
		switch (*p++) {
		case 'h':
			if (*p == 'h') p++, size = SIZE_hh;
			else size = SIZE_h;
			break;
		case 'l':
			if (*p == 'l') p++, size = SIZE_ll;
			else size = SIZE_l;
			break;
		case 'j': size = SIZE_ll; break;
		case 'z': case 't': size = SIZE_l; break;
		case 'L': size = SIZE_L; break;
		case 'd': case 'i': case 'o': case 'u': case 'x':
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': case 'X':
		case 's': case 'c': case '[': case 'S': case 'C':
		case 'p': case 'n':
			p--;
			break;
		default:
			goto fmt_fail;
		}

		t = *p;
		/* C and S are lc and ls */
		if ((t & 0x2f) == 3) { t |= 32; size = SIZE_l; }

		switch (t) {
		case 'c':
			if (width < 1) width = 1;
			/* fall through */
		case '[':
			break;
		case 'n':
			store_int(dest, size, (unsigned long long)pos);
			continue;
		default:
			shlim(f, 0);
			while (isspace_(shgetc(f)));
			shunget(f);
			pos += shcnt(f);
		}

		shlim(f, width);
		if (shgetc(f) < 0) goto input_fail;
		shunget(f);

		switch (t) {
		case 's': case 'c': case '[':
			if (t == 'c' || t == 's') {
				memset(scanset, -1, sizeof scanset);
				scanset[0] = 0;
				if (t == 's') {
					scanset[1 + '\t'] = scanset[1 + '\n'] = scanset[1 + '\v'] = 0;
					scanset[1 + '\f'] = scanset[1 + '\r'] = scanset[1 + ' '] = 0;
				}
			} else {
				if (*++p == '^') p++, invert = 1;
				else invert = 0;
				memset(scanset, invert, sizeof scanset);
				scanset[0] = 0;
				if (*p == '-') p++, scanset[1 + '-'] = (unsigned char)(1 - invert);
				else if (*p == ']') p++, scanset[1 + ']'] = (unsigned char)(1 - invert);
				for (; *p != ']'; p++) {
					if (!*p) goto fmt_fail;
					if (*p == '-' && p[1] && p[1] != ']') {   /* a range */
						int c;
						for (c = p++[-1]; c < *p; c++) scanset[1 + c] = (unsigned char)(1 - invert);
					}
					scanset[1 + *p] = (unsigned char)(1 - invert);
				}
			}
			wcs = 0;
			s = 0;
			i = 0;
			k = t == 'c' ? (size_t)width + 1U : 31;
			if (size == SIZE_l) {
				if (alloc) {
					wcs = malloc(k * sizeof(wchar_t));
					if (!wcs) goto alloc_fail;
				} else {
					wcs = dest;
				}
				{
					/* the bytes, as the scan set selects them, converted
					 * as multibyte characters of the locale */
					int c;
					mbstate_t st = { { 0, 0 } };
					const struct __lcat *ct = CURRENT_CTYPE;
					while (scanset[(c = shgetc(f)) + 1]) {
						char b = (char)c;
						wchar_t w;
						size_t r = __mbrtowc_l(&w, &b, 1, &st, ct);
						if (r == (size_t)-2) continue;
						if (r == (size_t)-1) goto match_fail;
						if (wcs) wcs[i] = w;
						i++;
						if (alloc && i == k) {
							wchar_t *tmp;
							k += k + 1;
							tmp = realloc(wcs, k * sizeof(wchar_t));
							if (!tmp) goto alloc_fail;
							wcs = tmp;
						}
					}
				}
			} else if (alloc) {
				int c;
				s = malloc(k);
				if (!s) goto alloc_fail;
				while (scanset[(c = shgetc(f)) + 1]) {
					s[i++] = (char)c;
					if (i == k) {
						char *tmp;
						k += k + 1;
						tmp = realloc(s, k);
						if (!tmp) goto alloc_fail;
						s = tmp;
					}
				}
			} else {
				int c;
				if ((s = dest)) {
					while (scanset[(c = shgetc(f)) + 1]) s[i++] = (char)c;
				} else {
					while (scanset[shgetc(f) + 1]);
				}
			}
			shunget(f);
			if (!shcnt(f)) goto match_fail;
			/* %c needs exactly width characters: fewer means end of input */
			if (t == 'c' && shcnt(f) != width) goto input_fail;
			if (alloc) {
				if (size == SIZE_l) *(wchar_t **)dest = wcs;
				else *(char **)dest = s;
			}
			if (t != 'c') {
				if (wcs) wcs[i] = 0;
				if (s) s[i] = 0;
			}
			break;
		case 'p':
		case 'X':
		case 'x':
			base = 16;
			goto int_common;
		case 'o':
			base = 8;
			goto int_common;
		case 'd':
		case 'u':
			base = 10;
			goto int_common;
		case 'i':
			base = 0;
		int_common:
			/* signed conversions saturate as strtoll does (as glibc) */
			x = __intscan(f, (unsigned)base, 0, t == 'd' || t == 'i' ? (unsigned long long)LLONG_MAX : ULLONG_MAX);
			if (!shcnt(f)) goto match_fail;
			if (t == 'p' && dest) *(void **)dest = (void *)(uintptr_t)x;
			else store_int(dest, size, x);
			break;
		case 'a': case 'A':
		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
			y = __floatscan(f, size == SIZE_def ? 0 : size == SIZE_l ? 1 : 2, 0);
			if (!shcnt(f)) goto match_fail;
			if (dest) switch (size) {
			case SIZE_def: *(float *)dest = (float)y; break;
			case SIZE_l:   *(double *)dest = (double)y; break;
			case SIZE_L:   *(long double *)dest = y; break;
			}
			break;
		}

		pos += shcnt(f);
		if (dest) matches++;
	}
	if (0) {
fmt_fail:
alloc_fail:
input_fail:
		if (!matches) matches--;
match_fail:
		if (alloc) {
			free(s);
			free(wcs);
		}
	}
	return matches;
}

int vfscanf(FILE *__RESTRICT f, const char *__RESTRICT fmt, va_list ap)
{
	int r;
	FLOCK(f);
	r = __vfscanf(f, fmt, ap);
	FUNLOCK(f);
	return r;
}

int fscanf(FILE *__RESTRICT f, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfscanf(f, fmt, ap); va_end(ap);
	return r;
}

int vscanf(const char *__RESTRICT fmt, va_list ap) { return vfscanf(stdin, fmt, ap); }

int scanf(const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vfscanf(stdin, fmt, ap); va_end(ap);
	return r;
}

/* sscanf: a stream reading from the string */
static size_t string_read(FILE *f, unsigned char *buf, size_t len)
{
	const char *src = f->cookie;
	size_t k = len + 256;
	const char *end = memchr(src, 0, k);
	if (end) k = (size_t)(end - src);
	if (k < len) len = k;
	memcpy(buf, src, len);
	f->rpos = (unsigned char *)(src + len);
	f->rend = (unsigned char *)(src + k);
	f->cookie = (void *)(src + k);
	return len;
}

int vsscanf(const char *__RESTRICT s, const char *__RESTRICT fmt, va_list ap)
{
	FILE f;
	memset(&f, 0, sizeof f);
	f.buf = (unsigned char *)s;
	f.cookie = (void *)s;
	f.read = string_read;
	f.lockcount = 1;
	f.fd = -1;
	f.lbf = -1;
	f.flags = F_NOWR;
	return __vfscanf(&f, fmt, ap);
}

int sscanf(const char *__RESTRICT s, const char *__RESTRICT fmt, ...)
{
	int r; va_list ap;
	va_start(ap, fmt); r = vsscanf(s, fmt, ap); va_end(ap);
	return r;
}

/* ---- the wide scanf engine: the input and the format are wide
 * characters (fields, widths and %n count characters); numbers are read
 * by __intscan and __floatscan through a stream that takes the wide
 * characters one at a time ---- */
#include <wchar.h>
#include <wctype.h>

struct wscan {
	struct __wsrc *in;
	off_t pos;           /* characters read */
	wint_t last;         /* the last one, for the number stream */
};

static wint_t wget(struct wscan *w)
{
	wint_t c = w->in->get(w->in);
	if (c != WEOF) w->pos++;
	return w->last = c;
}

static void wunget(struct wscan *w, wint_t c)
{
	if (c == WEOF) return;
	w->in->unget(w->in, c);
	w->pos--;
}

/* the number stream: one wide character per read, as its bytes in the
 * locale (non-ASCII ones matter only as a decimal point) */
static size_t num_read(FILE *f, unsigned char *buf, size_t len)
{
	struct wscan *w = f->cookie;
	wint_t c = wget(w);
	char mb[MB_LEN_MAX];
	mbstate_t st = { { 0, 0 } };
	size_t k;
	(void)len;
	if (c == WEOF) { f->flags |= F_EOF; return 0; }
	if (c < 0x80) { *buf = (unsigned char)c; return 1; }
	k = wcrtomb(mb, (wchar_t)c, &st);
	if (k == (size_t)-1 || !k) { *buf = 0xff; return 1; }
	*buf = (unsigned char)mb[0];
	memcpy(f->buf, mb + 1, k - 1);
	f->rpos = f->buf;
	f->rend = f->buf + (k - 1);
	return 1;
}

/* c in the scan set [set, end) of %[ (ranges by code point) */
static int inset(const wchar_t *set, const wchar_t *end, wint_t c)
{
	const wchar_t *q;
	for (q = set; q < end; q++) {
		if (q > set && *q == '-' && q + 1 < end) {
			if (c > (wint_t)q[-1] && c <= (wint_t)q[1]) return 1;
			q++;
			if ((wint_t)*q == c) return 1;
			continue;
		}
		if ((wint_t)*q == c) return 1;
	}
	return 0;
}

int __vfwscanf_core(struct __wsrc *in, const wchar_t *fmt, va_list ap)
{
	int width, size, alloc = 0, base, t, invert = 0, matches = 0;
	const wchar_t *p, *set = 0, *setend = 0;
	struct wscan w;
	size_t i, k;
	void *dest = 0;
	char *s = 0;
	wchar_t *wcs = 0;
	unsigned long long x;
	long double y;
	wint_t c;

	w.in = in; w.pos = 0; w.last = WEOF;
	for (p = fmt; *p; p++) {
		alloc = 0;

		if (iswspace((wint_t)*p)) {
			while (iswspace((wint_t)p[1])) p++;
			while (iswspace(c = wget(&w)));
			wunget(&w, c);
			continue;
		}
		if (*p != '%' || p[1] == '%') {
			if (*p == '%') {
				p++;
				while (iswspace(c = wget(&w)));
			} else
				c = wget(&w);
			if (c != (wint_t)*p) {
				wunget(&w, c);
				if (c == WEOF) goto input_fail;
				goto match_fail;
			}
			continue;
		}

		p++;
		if (*p == '*') {
			dest = 0; p++;
		} else if ((unsigned)*p - '0' < 10 && p[1] == '$') {
			dest = arg_n(ap, (unsigned)(*p - '0')); p += 2;
		} else if ((unsigned)*p - '0' < 10 && (unsigned)p[1] - '0' < 10 && p[2] == '$') {
			dest = arg_n(ap, (unsigned)((p[0] - '0') * 10 + p[1] - '0')); p += 3;
		} else {
			dest = va_arg(ap, void *);
		}

		for (width = 0; (unsigned)*p - '0' < 10; p++) width = 10 * width + (*p - '0');

		if (*p == 'm') { wcs = 0; s = 0; alloc = !!dest; p++; }

		size = SIZE_def;
		switch (*p++) {
		case 'h':
			if (*p == 'h') p++, size = SIZE_hh;
			else size = SIZE_h;
			break;
		case 'l':
			if (*p == 'l') p++, size = SIZE_ll;
			else size = SIZE_l;
			break;
		case 'j': size = SIZE_ll; break;
		case 'z': case 't': size = SIZE_l; break;
		case 'L': size = SIZE_L; break;
		case 'd': case 'i': case 'o': case 'u': case 'x':
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': case 'X':
		case 's': case 'c': case '[': case 'S': case 'C':
		case 'p': case 'n':
			p--;
			break;
		default:
			goto fmt_fail;
		}

		t = (int)*p;
		if ((t & 0x2f) == 3) { t |= 32; size = SIZE_l; }

		switch (t) {
		case 'c':
			if (width < 1) width = 1;
			/* fall through */
		case '[':
			break;
		case 'n':
			store_int(dest, size, (unsigned long long)w.pos);
			continue;
		default:
			while (iswspace(c = wget(&w)));
			wunget(&w, c);
		}

		if ((c = wget(&w)) == WEOF) goto input_fail;
		wunget(&w, c);

		switch (t) {
		case 's': case 'c': case '[':
			if (t == '[') {
				if (*++p == '^') p++, invert = 1;
				else invert = 0;
				set = p;
				if (*p == ']') p++;
				while (*p != ']') if (!*p++) goto fmt_fail;
				setend = p;
			}
			wcs = 0;
			s = 0;
			i = 0;
			k = t == 'c' ? (size_t)width + 1U : 31;
			if (alloc) {
				if (size == SIZE_l) { if (!(wcs = malloc(k * sizeof(wchar_t)))) goto alloc_fail; }
				else if (!(s = malloc(k))) goto alloc_fail;
			} else if (size == SIZE_l) wcs = dest;
			else s = dest;
			{
				int n = 0;
				mbstate_t st = { { 0, 0 } };
				for (; !width || n < width; n++) {
					if ((c = wget(&w)) == WEOF) break;
					if (t == 's' ? iswspace(c) : t == '[' && inset(set, setend, c) == invert) {
						wunget(&w, c);
						break;
					}
					if (wcs) {
						wcs[i++] = (wchar_t)c;
						if (alloc && i == k) {
							wchar_t *tmp;
							k += k + 1;
							if (!(tmp = realloc(wcs, k * sizeof(wchar_t)))) goto alloc_fail;
							wcs = tmp;
						}
					} else if (s) {
						char mb[MB_LEN_MAX];
						size_t l = wcrtomb(mb, (wchar_t)c, &st);
						/* (as glibc: %c fails as input, %s and %[ as a match) */
						if (l == (size_t)-1) { if (t == 'c') goto input_fail; goto match_fail; }
						if (alloc && i + l + 1 >= k) {
							char *tmp;
							k += k + l + 1;
							if (!(tmp = realloc(s, k))) goto alloc_fail;
							s = tmp;
						}
						memcpy(s + i, mb, l);
						i += l;
					}
				}
				if (!n) goto match_fail;
				if (t == 'c' && n != width) goto input_fail;
			}
			if (alloc) {
				if (size == SIZE_l) *(wchar_t **)dest = wcs;
				else *(char **)dest = s;
			}
			if (t != 'c') {
				if (wcs) wcs[i] = 0;
				if (s) s[i] = 0;
			}
			break;
		default: {
			/* numbers */
			unsigned char area[UNGET + MB_LEN_MAX];
			FILE f;
			memset(&f, 0, sizeof f);
			f.buf = area + UNGET;
			f.buf_size = MB_LEN_MAX;
			f.cookie = &w;
			f.read = num_read;
			f.lockcount = 1;
			f.fd = -1;
			f.lbf = -1;
			f.flags = F_NOWR;
			__toread(&f);
			f.rpos = f.rend = f.buf;
			shlim(&f, width);
			switch (t) {
			case 'p': case 'X': case 'x': base = 16; goto int_common;
			case 'o': base = 8; goto int_common;
			case 'd': case 'u': base = 10; goto int_common;
			case 'i':
				base = 0;
			int_common:
				x = __intscan(&f, (unsigned)base, 0, t == 'd' || t == 'i' ? (unsigned long long)LLONG_MAX : ULLONG_MAX);
				y = 0;
				break;
			default:
				y = __floatscan(&f, size == SIZE_def ? 0 : size == SIZE_l ? 1 : 2, 0);
				x = 0;
			}
			/* the character read ahead (or a decimal point in part) */
			if (f.rend > f.rpos) wunget(&w, w.last);
			if (!shcnt(&f)) goto match_fail;
			if (t == 'p') { if (dest) *(void **)dest = (void *)(uintptr_t)x; }
			else if (t == 'd' || t == 'i' || t == 'o' || t == 'u' || t == 'x' || t == 'X') store_int(dest, size, x);
			else if (dest) switch (size) {
			case SIZE_def: *(float *)dest = (float)y; break;
			case SIZE_l:   *(double *)dest = (double)y; break;
			case SIZE_L:   *(long double *)dest = y; break;
			}
		}
		}
		if (dest) matches++;
	}
	if (0) {
fmt_fail:
alloc_fail:
input_fail:
		if (!matches) matches--;
match_fail:
		if (alloc) {
			free(s);
			free(wcs);
		}
	}
	return matches;
}

/* mkuctype - make src/locale/uctype_data.h, the character classes, case
 * mappings and widths of the built-in C.UTF-8 locale, from an LC_CTYPE
 * file in the GNU C library's format (as the localedef of the POSIX
 * utilities writes it), e.g. /usr/lib/locale/C.utf8/LC_CTYPE.
 *
 *   cc -o mkuctype tools/mkuctype.c && ./mkuctype LC_CTYPE src/locale
 *
 * writes uctype_class.h, uctype_map.h and uctype_width.h there.
 *
 * A maintainer's tool: the generated file is part of the sources. The
 * tables are lists of ranges, checked here against the input for every
 * code point. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAXC 0x110000

static unsigned char *d;
static uint32_t n, *offs;

static uint32_t u32(uint32_t off) { uint32_t v; memcpy(&v, d + off, 4); return v; }

static int bit3(uint32_t t, uint32_t wc)
{
	uint32_t s1 = u32(t), b = u32(t + 4), s2 = u32(t + 8), m2 = u32(t + 12), m3 = u32(t + 16), i1, l1, l2, l3;
	if ((i1 = wc >> s1) >= b) return 0;
	if (!(l1 = u32(t + 20 + 4 * i1))) return 0;
	if (!(l2 = u32(t + l1 + 4 * ((wc >> s2) & m2)))) return 0;
	l3 = u32(t + l2 + 4 * ((wc >> 5) & m3));
	return (l3 >> (wc & 31)) & 1;
}

static int32_t map3(uint32_t t, uint32_t wc)
{
	uint32_t s1 = u32(t), b = u32(t + 4), s2 = u32(t + 8), m2 = u32(t + 12), m3 = u32(t + 16), i1, l1, l2;
	if ((i1 = wc >> s1) >= b) return 0;
	if (!(l1 = u32(t + 20 + 4 * i1))) return 0;
	if (!(l2 = u32(t + l1 + 4 * ((wc >> s2) & m2)))) return 0;
	return (int32_t)u32(t + l2 + 4 * (wc & m3));
}

static int byte3(uint32_t t, uint32_t wc)
{
	uint32_t s1 = u32(t), b = u32(t + 4), s2 = u32(t + 8), m2 = u32(t + 12), m3 = u32(t + 16), i1, l1, l2;
	if ((i1 = wc >> s1) >= b) return 0xff;
	if (!(l1 = u32(t + 20 + 4 * i1))) return 0xff;
	if (!(l2 = u32(t + l1 + 4 * ((wc >> s2) & m2)))) return 0xff;
	return d[t + l2 + (wc & m3)];
}

static const char *const std[] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph",
	"lower", "print", "punct", "space", "upper", "xdigit" };

static void ranges(const char *name, int (*f)(uint32_t))
{
	uint32_t c = 0, s, k = 0;
	printf("static const uint32_t %s[] = {", name);
	while (c < MAXC) {
		if (!f(c)) { c++; continue; }
		s = c;
		while (c < MAXC && f(c)) c++;
		printf("%s0x%x,0x%x,", k++ % 4 ? " " : "\n\t", s, c - 1);
	}
	printf("\n};\n");
}

static uint32_t cur;
static int isclass(uint32_t c) { return bit3(cur, c); }
static uint32_t wtab;
static int zerow(uint32_t c) { return byte3(wtab, c) == 0; }
static int wide(uint32_t c) { return byte3(wtab, c) == 2; }
static int printable(uint32_t c);
static uint32_t printtab;
static int printable(uint32_t c) { return bit3(printtab, c); }

/* case mapping: runs (start, count, delta, step 1 or 2); looked up as in
 * src/locale/uctype_map.c, which is checked here */
static int32_t runs[4 * MAXC];
static size_t nruns;

static int32_t lookup(uint32_t c)
{
	size_t lo = 0, hi = nruns, i, k;
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		if ((uint32_t)runs[4 * m] <= c) lo = m + 1; else hi = m;
	}
	for (i = lo, k = 0; i > 0 && k < 4; i--, k++) {
		const int32_t *r = runs + 4 * (i - 1);
		uint32_t d = c - (uint32_t)r[0];
		if (d < (uint32_t)(r[1] * r[3]) && d % (uint32_t)r[3] == 0) return r[2];
	}
	return 0;
}

static void maps(const char *name, uint32_t t)
{
	uint32_t c = 0, k = 0;
	nruns = 0;
	printf("static const int32_t %s[] = {", name);
	while (c < MAXC) {
		int32_t dl = map3(t, c);
		uint32_t s = c, cnt = 1, step;
		if (!dl) { c++; continue; }
		/* step 2 if the next one is unmapped and the one after has the same delta */
		step = (c + 2 < MAXC && map3(t, c + 1) != dl && map3(t, c + 2) == dl) ? 2 : 1;
		while (c + step < MAXC && map3(t, c + step) == dl && (step == 1 || map3(t, c + 1) != dl)) { c += step; cnt++; }
		printf("%s0x%x,%u,%d,%u,", k++ % 3 ? " " : "\n\t", s, cnt, dl, step);
		runs[4 * nruns] = (int32_t)s; runs[4 * nruns + 1] = (int32_t)cnt;
		runs[4 * nruns + 2] = dl; runs[4 * nruns + 3] = (int32_t)step;
		nruns++;
		c++;
	}
	printf("\n};\n");
	for (c = 0; c < MAXC; c++)
		if (lookup(c) != map3(t, c)) { fprintf(stderr, "%s: lookup fails at %x\n", name, c); exit(1); }
}

int main(int argc, char **argv)
{
	FILE *f;
	long len;
	uint32_t i, j, nclass = 0, nmap = 0, cls[12] = { 0 }, up = 0, lo = 0;
	const char *p;
	char out[512];
	if (argc != 3 || !(f = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: mkuctype LC_CTYPE dir\n"); return 2; }
	fseek(f, 0, SEEK_END); len = ftell(f); rewind(f);
	d = malloc((size_t)len);
	if (fread(d, 1, (size_t)len, f) != (size_t)len) return 1;
	n = u32(4);
	offs = (uint32_t *)(void *)(d + 8);
	for (p = (const char *)d + offs[10]; *p; p += strlen(p) + 1) nclass++;
	for (p = (const char *)d + offs[11]; *p; p += strlen(p) + 1) nmap++;
	for (i = 0, p = (const char *)d + offs[10]; i < nclass; i++, p += strlen(p) + 1)
		for (j = 0; j < 12; j++) if (!strcmp(p, std[j])) cls[j] = offs[72 + i];
	for (i = 0, p = (const char *)d + offs[11]; i < nmap; i++, p += strlen(p) + 1) {
		if (!strcmp(p, "toupper")) up = offs[72 + nclass + i];
		if (!strcmp(p, "tolower")) lo = offs[72 + nclass + i];
	}
	wtab = offs[12];
	printtab = cls[7];
#define OPEN(nm) do { snprintf(out, sizeof out, "%s/%s", argv[2], nm); \
		if (!freopen(out, "w", stdout)) return 1; \
		printf("/* Generated by tools/mkuctype.c from %s: do not edit. */\n", argv[1]); } while (0)
	OPEN("uctype_class.h");
	printf("/* Classes: ranges first,last. */\n");
	/* alnum, graph and punct follow: alpha|digit, print&~space, graph&~alnum
	 * (checked below) */
	for (j = 0; j < 12; j++) {
		char nm[32];
		if (!strcmp(std[j], "alnum") || !strcmp(std[j], "graph") || !strcmp(std[j], "punct")) continue;
		snprintf(nm, sizeof nm, "u_%s", std[j]);
		cur = cls[j];
		ranges(nm, isclass);
	}
	OPEN("uctype_map.h");
	printf("/* Case maps: start, count, delta, step. */\n");
	maps("u_toupper", up);
	maps("u_tolower", lo);
	OPEN("uctype_width.h");
	printf("/* Ranges of the characters of width 0 and of width 2. */\n");
	ranges("u_width0", zerow);
	ranges("u_width2", wide);
	for (i = 0; i < MAXC; i++) {
		int al = bit3(cls[1], i), dg = bit3(cls[4], i), pr = bit3(cls[7], i), sp = bit3(cls[9], i);
		if (bit3(cls[0], i) != (al | dg) || bit3(cls[5], i) != (pr & !sp) || bit3(cls[8], i) != (pr & !sp & !(al | dg))) {
			fprintf(stderr, "derived class mismatch at %x\n", i);
			return 1;
		}
	}
	/* glibc: width 0xff (and missing) is -1; that must be exactly the non-printables */
	for (i = 0; i < MAXC; i++) {
		int w = byte3(wtab, i);
		if ((w == 0xff) != !printable(i) && i) { fprintf(stderr, "width/print mismatch at %x\n", i); return 1; }
	}
	return 0;
}

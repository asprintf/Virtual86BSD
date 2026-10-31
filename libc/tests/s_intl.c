/* Self-checking: locale objects (newlocale, uselocale, duplocale,
 * getlocalename_l, the _l functions), setlocale's names, message
 * catalogs (catopen/catgets) and message objects (gettext, plurals,
 * codeset conversion), made here in the formats gencat and msgfmt write;
 * wide-character streams and memory streams. */
#define _XOPEN_SOURCE 800
#include <locale.h>
#include <langinfo.h>
#include <nl_types.h>
#include <libintl.h>
#include <wchar.h>
#include <wctype.h>
#include <uchar.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include "out.h"

static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }

/* a catalog: set 1 message 1 "one", set 1 message 2 "two", set 2 message 5 "five" */
static void mkcat(const char *path)
{
	static const struct { uint32_t set, msg; const char *t; } m[] = { { 1, 1, "one" }, { 1, 2, "two" }, { 2, 5, "five" } };
	unsigned char buf[512];
	uint32_t size = 3, depth = 1, off = 0, i;
	size_t ent = size * depth, n;
	int fd;
	memset(buf, 0, sizeof buf);
	put32(buf, 0x960408de); put32(buf + 4, size); put32(buf + 8, depth);
	for (i = 0; i < 3; i++) {
		uint32_t slot = ((m[i].set + 1) * m[i].msg) % size;
		unsigned char *e = buf + 12 + 12 * slot;
		put32(e, m[i].set + 1); put32(e + 4, m[i].msg); put32(e + 8, off);
		memcpy(buf + 12 + 24 * ent + off, m[i].t, strlen(m[i].t) + 1);
		off += (uint32_t)strlen(m[i].t) + 1;
	}
	n = 12 + 24 * ent + off;
	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	CHECK(fd >= 0 && write(fd, buf, n) == (ssize_t)n);
	close(fd);
}

/* a message object in ISO-8859-1, with plural forms (sorted originals) */
static void mkmo(const char *path)
{
	static const char *const orig[] = { "", "Hello", "file\0files", "size" };
	static const size_t olen[] = { 0, 5, 10, 4 };
	static const char *const tr[] = {
		"Content-Type: text/plain; charset=ISO-8859-1\nPlural-Forms: nplurals=3; plural=n==1 ? 0 : n%10>=2 && n%10<=4 ? 1 : 2;\n",
		"Hallo", "bestand\0bestandjes\0bestanden", "Gr\xf6\xdf" "e" };
	static const size_t tlen[] = { 0, 5, 28, 5 };
	unsigned char buf[1024];
	uint32_t n = 4, o = 28, t = 28 + 8 * n, s = 28 + 16 * n, i;
	int fd;
	memset(buf, 0, sizeof buf);
	put32(buf, 0x950412de); put32(buf + 4, 0); put32(buf + 8, n); put32(buf + 12, o); put32(buf + 16, t);
	for (i = 0; i < n; i++) {
		put32(buf + o + 8 * i, (uint32_t)olen[i]); put32(buf + o + 8 * i + 4, s);
		memcpy(buf + s, orig[i], olen[i]); s += (uint32_t)olen[i] + 1;
	}
	for (i = 0; i < n; i++) {
		size_t l = i ? tlen[i] : strlen(tr[0]);
		put32(buf + t + 8 * i, (uint32_t)l); put32(buf + t + 8 * i + 4, s);
		memcpy(buf + s, tr[i], l); s += (uint32_t)l + 1;
	}
	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	CHECK(fd >= 0 && write(fd, buf, s) == (ssize_t)s);
	close(fd);
}

int main(void)
{
	char dir[64] = "/tmp/libc-sintl.XXXXXX", path[256], *r;
	locale_t a, b, old;
	nl_catd cd;

	/* setlocale: names, composites, failures leave things as they were */
	CHECK(!strcmp(setlocale(LC_ALL, 0), "C"));
	CHECK(setlocale(LC_ALL, "C.UTF-8") && !strcmp(setlocale(LC_ALL, 0), "C.UTF-8") && MB_CUR_MAX > 1);
	CHECK(setlocale(LC_NUMERIC, "POSIX") && !strcmp(setlocale(LC_NUMERIC, 0), "C"));   /* POSIX is C */
	r = setlocale(LC_ALL, 0);
	CHECK(r && strstr(r, "LC_CTYPE=C.UTF-8;") && strstr(r, "LC_NUMERIC=C"));
	{ char save[512]; strcpy(save, r); CHECK(setlocale(LC_ALL, "C") && setlocale(LC_ALL, save) && !strcmp(setlocale(LC_ALL, 0), save)); }
	CHECK(setlocale(LC_ALL, "no_SUCH.locale") == 0 && !strcmp(setlocale(LC_CTYPE, 0), "C.UTF-8"));
	CHECK(setlocale(LC_ALL, "en_ZZ.UTF-8") != 0 && !strcmp(nl_langinfo(CODESET), "UTF-8"));   /* a UTF-8 name without data */
	setenv("LC_ALL", "", 1); setenv("LC_CTYPE", "C.UTF-8", 1); setenv("LANG", "POSIX", 1);
	CHECK(setlocale(LC_ALL, "") && !strcmp(setlocale(LC_CTYPE, 0), "C.UTF-8") && !strcmp(setlocale(LC_TIME, 0), "C"));
	setenv("LC_ALL", "C", 1);
	CHECK(setlocale(LC_ALL, "") && !strcmp(setlocale(LC_ALL, 0), "C") && MB_CUR_MAX == 1);
	CHECK(setlocale(99, "C") == 0);

	/* locale objects */
	a = newlocale(LC_CTYPE_MASK, "C.UTF-8", 0);
	CHECK(a != 0);
	b = newlocale(LC_TIME_MASK, "C", a);
	CHECK(b == a);
	CHECK(!strcmp(getlocalename_l(LC_CTYPE, a), "C.UTF-8") && !strcmp(getlocalename_l(LC_NUMERIC, a), "C"));
	CHECK(newlocale(LC_ALL_MASK, "no_SUCH", 0) == 0 && errno == ENOENT);
	CHECK(newlocale(1 << 12, "C", 0) == 0 && errno == EINVAL);
	CHECK(!strcmp(nl_langinfo_l(CODESET, a), "UTF-8") && !strcmp(nl_langinfo(CODESET), "ANSI_X3.4-1968"));
	CHECK(iswalpha_l(0xe9, a) && !iswalpha(0xe9) && towupper_l(0xe9, a) == 0xc9 && towupper(0xe9) == 0xe9);
	old = uselocale(a);
	CHECK(old == LC_GLOBAL_LOCALE && uselocale(0) == a);
	CHECK(MB_CUR_MAX > 1 && iswalpha(0xe9) && mblen("\xc3\xa9", 2) == 2);
	{ char b8[8]; mbstate_t st; memset(&st, 0, sizeof st); CHECK(wcrtomb(b8, 0x20ac, &st) == 3 && !memcmp(b8, "\xe2\x82\xac", 3)); }
	{ locale_t d = duplocale(LC_GLOBAL_LOCALE); CHECK(d && !strcmp(getlocalename_l(LC_CTYPE, d), "C")); freelocale(d); }
	{ locale_t d = duplocale(a); CHECK(d && d != a && !strcmp(getlocalename_l(LC_CTYPE, d), "C.UTF-8")); freelocale(d); }
	CHECK(uselocale(LC_GLOBAL_LOCALE) == a && MB_CUR_MAX == 1);
	CHECK(isalpha_l('a', a) && toupper_l('a', a) == 'A' && !isalpha_l(0xe9, a));
	freelocale(a);

	/* UTF-16 conversions */
	setlocale(LC_ALL, "C.UTF-8");
	{
		char16_t c16[4];
		mbstate_t st;
		size_t k;
		char mb[8];
		memset(&st, 0, sizeof st);
		CHECK(mbrtoc16(c16, "\xf0\x9f\x98\x80", 4, &st) == 4 && c16[0] == 0xd83d);
		CHECK(mbrtoc16(c16 + 1, "", 0, &st) == (size_t)-3 && c16[1] == 0xde00 && mbsinit(&st));
		memset(&st, 0, sizeof st);
		CHECK(c16rtomb(mb, 0xd83d, &st) == 0);
		k = c16rtomb(mb, 0xde00, &st);
		CHECK(k == 4 && !memcmp(mb, "\xf0\x9f\x98\x80", 4));
		CHECK(c16rtomb(mb, 0xde00, &st) == (size_t)-1 && errno == EILSEQ);
		memset(&st, 0, sizeof st);
		{ char32_t c32; CHECK(mbrtoc32(&c32, "\xe2\x82\xac", 3, &st) == 3 && c32 == 0x20ac); }
		CHECK(mbrtowc(0, "\xe2\x82", 2, &st) == (size_t)-2 && !mbsinit(&st));
		CHECK(mbrtowc(0, "\xac", 1, &st) == 1 && mbsinit(&st));
	}

	CHECK(mkdtemp(dir) != 0);

	/* catalogs */
	strcpy(path, dir); strcat(path, "/test.cat");
	mkcat(path);
	cd = catopen(path, 0);
	CHECK(cd != (nl_catd)-1);
	CHECK(!strcmp(catgets(cd, 1, 1, "x"), "one") && !strcmp(catgets(cd, 1, 2, "x"), "two"));
	CHECK(!strcmp(catgets(cd, 2, 5, "x"), "five"));
	errno = 0; CHECK(!strcmp(catgets(cd, 2, 6, "dflt"), "dflt") && errno == ENOMSG);
	CHECK(catclose(cd) == 0);
	{
		char spec[200];
		strcpy(spec, dir); strcat(spec, "/%N.cat");
		setenv("NLSPATH", spec, 1);
		cd = catopen("test", NL_CAT_LOCALE);
		CHECK(cd != (nl_catd)-1 && !strcmp(catgets(cd, 1, 2, "x"), "two"));
		catclose(cd);
		CHECK(catopen("nonexistent", 0) == (nl_catd)-1);
		unsetenv("NLSPATH");
	}

	/* gettext: dir/nl/LC_MESSAGES/demo.mo */
	strcpy(path, dir); strcat(path, "/nl"); mkdir(path, 0755);
	strcat(path, "/LC_MESSAGES"); mkdir(path, 0755);
	strcat(path, "/demo.mo");
	mkmo(path);
	CHECK(!strcmp(bindtextdomain("demo", dir), dir));
	CHECK(!strcmp(textdomain("demo"), "demo") && !strcmp(textdomain(0), "demo"));
	unsetenv("LANGUAGE");
	setlocale(LC_ALL, "C");
	CHECK(!strcmp(gettext("Hello"), "Hello"));      /* the C locale: untranslated */
	CHECK(setlocale(LC_ALL, "nl.UTF-8") != 0);       /* (no locale data: taken as C.UTF-8) */
	CHECK(!strcmp(gettext("Hello"), "Hallo"));
	CHECK(!strcmp(gettext("size"), "Gr\xc3\xb6\xc3\x9f" "e"));           /* converted to UTF-8 */
	CHECK(!strcmp(ngettext("file", "files", 1), "bestand"));
	CHECK(!strcmp(ngettext("file", "files", 3), "bestandjes"));
	CHECK(!strcmp(ngettext("file", "files", 5), "bestanden"));
	CHECK(!strcmp(ngettext("nope", "nopes", 5), "nopes") && !strcmp(gettext("nope"), "nope"));
	CHECK(!strcmp(dgettext("other", "Hello"), "Hello"));
	bind_textdomain_codeset("demo", "ISO-8859-1");
	CHECK(!strcmp(dgettext("demo", "size"), "Gr\xf6\xdf" "e"));
	setenv("LANGUAGE", "xx:nl", 1);
	setlocale(LC_ALL, "de.UTF-8");
	CHECK(!strcmp(gettext("Hello"), "Hallo"));
	unsetenv("LANGUAGE");
	CHECK(!strcmp(gettext("Hello"), "Hello"));
	CHECK(textdomain("") && !strcmp(textdomain(0), "messages"));

	/* wide streams */
	setlocale(LC_ALL, "C.UTF-8");
	{
		FILE *f = tmpfile();
		wchar_t line[32];
		CHECK(f && fwide(f, 0) == 0);
		CHECK(fputws(L"gr\x00fc\x00df \x20ac\n2", f) >= 0 && fwide(f, 0) > 0);
		CHECK(fputwc(0x3a9, f) == 0x3a9);
		rewind(f);
		CHECK(fgetwc(f) == 'g' && fgetwc(f) == 'r' && fgetwc(f) == 0xfc);
		CHECK(ungetwc(0x20ac, f) == 0x20ac && fgetwc(f) == 0x20ac && fgetwc(f) == 0xdf);
		CHECK(fgetws(line, 32, f) && !wcscmp(line, L" \x20ac\n"));
		CHECK(fgetwc(f) == '2' && fgetwc(f) == 0x3a9 && fgetwc(f) == WEOF && feof(f));
		fclose(f);
		f = tmpfile();
		fwrite("a\xff" "b", 1, 3, f);
		rewind(f);
		CHECK(fgetwc(f) == 'a' && fgetwc(f) == WEOF && errno == EILSEQ && ferror(f));
		fclose(f);
	}
	{
		wchar_t *wb;
		size_t ws;
		FILE *f = open_wmemstream(&wb, &ws);
		CHECK(f != 0);
		fwprintf(f, L"%d %ls %s", 42, L"\x00e9t\x00e9", "\xc3\xa0");
		fflush(f);
		CHECK(ws == 8 && !wcscmp(wb, L"42 \x00e9t\x00e9 \x00e0"));
		fseek(f, 3, SEEK_SET);
		fputwc('E', f);
		fclose(f);
		CHECK(!wcscmp(wb, L"42 Et\x00e9 \x00e0") && ws == 4);
		free(wb);
	}
	{
		wchar_t w[64];
		struct tm tm = { .tm_year = 124, .tm_mon = 0, .tm_mday = 2, .tm_wday = 2 };
		CHECK(wcsftime(w, 64, L"%A %d \x00e9 %B", &tm) == 20 && !wcscmp(w, L"Tuesday 02 \x00e9 January"));
		CHECK(wcsftime(w, 5, L"%A", &tm) == 0);
		CHECK(swprintf(w, 64, L"%5.2ls|%-4lc|%3d", L"\x00e9\x00e8x", (wint_t)0x20ac, 7) == 14 &&
		      !wcscmp(w, L"   \x00e9\x00e8|\x20ac   |  7"));
		CHECK(swprintf(w, 4, L"%s", "toolong") == -1);
	}

	{ char cmd[100] = "rm -rf "; strcat(cmd, dir); CHECK(system(cmd) == 0); }
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}

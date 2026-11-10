/* Differential: regcomp/regexec (whole matches of random patterns; glibc
 * does not follow POSIX for subexpressions, see s_regex.c), fnmatch
 * (random patterns and names), glob (a directory made here), libgen. */
#define _XOPEN_SOURCE 800
#include <regex.h>
#include <fnmatch.h>
#include <glob.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "out.h"

static char *pp;
static int ngrp;

static unsigned r(unsigned n) { return (unsigned)(rng() % n); }

/* a random RE; quantifiers are not nested (glibc can take exponential
 * time on those) */
static int gen(int d, int ere)
{
	int k = d > 3 ? (int)r(4) : (int)r(10), q = 0;
	switch (k) {
	case 0: case 1: case 9: *pp++ = "ab"[r(2)]; break;
	case 2: *pp++ = '.'; break;
	case 3: strcpy(pp, r(2) ? "[ab]" : "[^a]"); pp += 4; break;
	case 4: case 5: q = gen(d + 1, ere); q |= gen(d + 1, ere); return q;
	case 6:
		ngrp++;
		if (ere) { *pp++ = '('; q = gen(d + 1, ere); *pp++ = '|'; q |= gen(d + 1, ere); *pp++ = ')'; }
		else { *pp++ = '\\'; *pp++ = '('; q = gen(d + 1, ere); *pp++ = '\\'; *pp++ = ')'; }
		break;
	default:
		ngrp++;
		if (ere) *pp++ = '('; else { *pp++ = '\\'; *pp++ = '('; }
		q = gen(d + 1, ere);
		if (ere) *pp++ = ')'; else { *pp++ = '\\'; *pp++ = ')'; }
		break;
	}
	if (q) return q;
	k = (int)r(8);
	if (k < 4) q = 1;
	if (k == 0) *pp++ = '*';
	else if (k == 1 && ere) *pp++ = '+';
	else if (k == 2 && ere) *pp++ = '?';
	else if (k == 3) {
		const char *f = ere ? "{%,%}" : "\\{%,%\\}";
		int a = (int)r(2), b = 1 + (int)r(2);
		for (; *f; f++) *pp++ = *f == '%' ? (char)('0' + (f[-1] == '{' ? a : b)) : *f;
	}
	return q;
}

static void re1(const char *pat, int cf, const char *s, int ef)
{
	regex_t re;
	regmatch_t m[1];
	int e = regcomp(&re, pat, cf);
	os(pat); oc(' '); os(s); os(": ");
	if (e) { os("error "); oi(e); onl(); return; }
	e = regexec(&re, s, 1, m, ef);
	if (e) os("nomatch");
	else { oc('('); oi(m[0].rm_so); oc(','); oi(m[0].rm_eo); oc(')'); }
	onl();
	regfree(&re);
}

static int cmpp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static const char *fnp[] = { "*", "?", "[ab]", "[!a]", "a", "b", ".", "/", "\\*", "[.]", "*a*", "[[:alpha:]]", "\\" };

int main(void)
{
	char pat[512], s[32], buf[64];
	int i, j, n;
	static const char *const fixed[][2] = {
		{ "a\\{2,3\\}", "aaaa" }, { "^*ab", "*ab" }, { "\\(a\\)\\1", "xaa" }, { "x\\{1", "x" },
		{ "a\\{2,1\\}", "a" }, { "[[:foo:]]", "a" }, { "[z-a]", "a" }, { "a\\", "a" }, { "\\(a", "a" },
		{ "a\\)", "a" }, { "[a", "a" }, { "[]a]", "]" }, { "[^]a]", "b" }, { "[a-]", "-" },
		{ "[[.a.]-c]", "b" }, { "[[=a=]]", "a" }, { "\\(.*\\)\\1", "abcabc" }, { "a*b*c", "xc" },
		{ "\\.\\[\\*", ".[*" }, { "$a", "$a" }, { "a^", "a^" }, { "\\(^a\\)", "a" }, { "x$\\|y", "y" },
	};
	static const char *const efixed[][2] = {
		{ "(", "" }, { ")", ")" }, { "a{,2}", "aaa" }, { "*a", "a" }, { "a|*", "a" }, { "a**", "aa" },
		{ "a{1", "a" }, { "()", "x" }, { "a|", "b" }, { "^a|b$", "cb" }, { "[[:digit:]]+", "ab123" },
		{ "(a|ab)(c|bcd)", "abcd" }, { "x+y?z*", "xxz" }, { "\\(", "(" },
	};

	for (i = 0; i < (int)(sizeof fixed / sizeof *fixed); i++) re1(fixed[i][0], 0, fixed[i][1], 0);
	for (i = 0; i < (int)(sizeof efixed / sizeof *efixed); i++) re1(efixed[i][0], REG_EXTENDED, efixed[i][1], 0);
	re1("ABC", REG_ICASE, "xabcx", 0);
	re1("[A-C]+", REG_ICASE | REG_EXTENDED, "xabcx", 0);
	re1("^b", REG_NEWLINE, "a\nb", 0);
	re1("a$", REG_NEWLINE, "a\nb", 0);
	re1("a.b", REG_NEWLINE, "a\nb", 0);
	re1("a[^x]b", REG_NEWLINE, "a\nb", 0);
	re1("a.b", 0, "a\nb", 0);
	re1("^a", 0, "a", REG_NOTBOL);
	re1("a$", 0, "a", REG_NOTEOL);
	re1("^a", REG_NEWLINE, "b\na", REG_NOTBOL);
	re1("", 0, "abc", 0);
	re1("", REG_EXTENDED, "", 0);
	/* the GNU extensions glibc has (and sed scripts use) */
	{
		static const char *const gnu[][2] = {
			{ "\\<b", "foo bar" }, { "o\\>", "foo bar" }, { "\\bba", "abar bar" }, { "a\\B", "a ab" },
			{ "\\w\\+", "  hello_1 x" }, { "\\W\\+", "ab  .cd" }, { "\\s\\+", "ab \t cd" }, { "\\S\\+", "  xyz " },
			{ "\\`a", "aa" }, { "a\\'", "aa" }, { "\\(\\<x\\)\\1", "a xx" }, { "\\<", "   " }, { "\\B", "ab" },
			{ "x\\b", "x" }, { "\\>a", "a" },
		};
		for (i = 0; i < (int)(sizeof gnu / sizeof *gnu); i++) {
			re1(gnu[i][0], 0, gnu[i][1], 0);
			re1(gnu[i][0], REG_EXTENDED, gnu[i][1], 0);
		}
	}
	for (i = 0; i < 20000; i++) {
		int ere = (int)r(2), len = (int)r(9);
		pp = pat; ngrp = 0;
		if (!r(5)) *pp++ = '^';
		gen(0, ere);
		if (!r(6)) *pp++ = '$';
		*pp = 0;
		for (j = 0; j < len; j++) s[j] = "abc"[r(3)];
		s[len] = 0;
		re1(pat, ere ? REG_EXTENDED : 0, s, 0);
	}
	{   /* regerror for every code, short buffers */
		for (i = 0; i <= 13; i++) {
			oi((long long)regerror(i, 0, buf, sizeof buf)); oc(' '); os(buf);
			regerror(i, 0, buf, 5); oc(' '); os(buf); onl();
		}
	}

	/* fnmatch */
	for (i = 0; i < 30000; i++) {
		int fl = (int)r(16) & (FNM_NOESCAPE | FNM_PATHNAME | FNM_PERIOD), k;
		n = 1 + (int)r(4);
		pat[0] = 0;
		for (j = 0; j < n; j++) {
			k = (int)r(sizeof fnp / sizeof *fnp);
			if (k == 12 && j != n - 1) k = 0;   /* a trailing backslash only */
			strcat(pat, fnp[k]);
		}
		n = (int)r(5);
		for (j = 0; j < n; j++) s[j] = "ab./*"[r(5)];
		s[n] = 0;
		if (!strcmp(fnp[12], pat + strlen(pat) - 1) && pat[strlen(pat) - 2] != '\\') continue;  /* unspecified */
		/* glibc takes a period after a slash as leading even without
		 * FNM_PATHNAME */
		if ((fl & FNM_PERIOD) && !(fl & FNM_PATHNAME) && strstr(s, "/.")) continue;
		os(pat); oc(' '); os(s); oc(' '); oi(fl); oc(' '); oi(fnmatch(pat, s, fl) == 0); onl();
	}

	/* glob, in a directory of our own */
	{
		static const char *const files[] = { "a", "b", "ab", ".h", "d1/", "d1/x", "d1/y.c", "d2/", "d2/x", "d2/.z",
			"d2/sub/", "d2/sub/x", "c[1]", "e*" };
		static const char *const pats[] = { "*", ".*", "?", "d*/x", "d?/*", "*/x", "d2/*", "d2/.*", "*/", "d*/*/x",
			"[ab]*", "[!a]*", "nomatch*", "c[1]", "c\\[1]", "e\\*", "d1", "d1/", "*/*.c", "./a*", "d2/sub/../x", "" };
		char dir[64], cwd[512];
		glob_t g;
		int fl[] = { 0, GLOB_MARK, GLOB_NOCHECK, GLOB_NOESCAPE, GLOB_MARK | GLOB_NOSORT };
		strcpy(dir, "/tmp/libc-tglob.");
		{ char num[16], *q = num + sizeof num; unsigned u = (unsigned)getpid(); *--q = 0;
		  do *--q = (char)('0' + u % 10); while (u /= 10); strcat(dir, q); }
		getcwd(cwd, sizeof cwd);
		mkdir(dir, 0755);
		chdir(dir);
		for (i = 0; i < (int)(sizeof files / sizeof *files); i++) {
			size_t l = strlen(files[i]);
			if (files[i][l - 1] == '/') { strcpy(buf, files[i]); buf[l - 1] = 0; mkdir(buf, 0755); }
			else close(open(files[i], O_CREAT | O_WRONLY, 0644));
		}
		for (i = 0; i < (int)(sizeof pats / sizeof *pats); i++)
			for (j = 0; j < 5; j++) {
				int e;
				size_t k;
				/* glibc appends a second slash to "d1/" for "*\/" (but not for "d1/") */
				if (!strcmp(pats[i], "*/") && (fl[j] & GLOB_MARK)) continue;
				e = glob(pats[i], fl[j], 0, &g);
				os(pats[i]); oc(' '); oi(fl[j]); os(": "); oi(e);
				if (!e) {
					char **v = g.gl_pathv;
					if (fl[j] & GLOB_NOSORT) qsort(v, g.gl_pathc, sizeof *v, cmpp);
					for (k = 0; k < g.gl_pathc; k++) { oc(' '); os(v[k]); }
					globfree(&g);
				}
				onl();
			}
		/* GLOB_DOOFFS and GLOB_APPEND */
		g.gl_offs = 2;
		glob("a*", GLOB_DOOFFS, 0, &g);
		glob("d1/*", GLOB_DOOFFS | GLOB_APPEND, 0, &g);
		oi((long long)g.gl_pathc); oi(g.gl_pathv[0] == 0); oi(g.gl_pathv[1] == 0);
		for (i = 0; i < (int)g.gl_pathc; i++) { oc(' '); os(g.gl_pathv[2 + i]); }
		oi(g.gl_pathv[2 + g.gl_pathc] == 0); onl();
		globfree(&g);
		chdir(cwd);
		strcpy(pat, "rm -rf "); strcat(pat, dir); system(pat);
	}

	/* basename, dirname ("//" is implementation-defined) */
	{
		static const char *const p[] = { "usr", "/usr/lib", "/usr/", "/", "///", ".", "..", "a//b//", "", "/a", "a/" };
		for (i = 0; i < (int)(sizeof p / sizeof *p); i++) {
			strcpy(buf, p[i]); os(basename(buf)); oc(' ');
			strcpy(buf, p[i]); os(dirname(buf)); onl();
		}
	}
	oflush();
	return 0;
}

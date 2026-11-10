/* Self-checking: multi-character collating elements in bracket
 * expressions of regcomp() and fnmatch(), in the locales made from
 * collate.def (a contraction "ch" between c and d) by run.sh.  glibc
 * departs here ([=ch=] never matches; REG_ICASE refuses [.ch.]; its
 * fnmatch commits to the first alternative), so this is not differential.
 * See regcomp.c: elements named by [.ch.] or of an equivalence class;
 * ranges and non-matching lists match single characters. */
#define _XOPEN_SOURCE 800
#include <regex.h>
#include <fnmatch.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

/* expect: "so,eo" of the whole match, "-" (no match), or "E" (regcomp fails) */
static void r(int line, const char *pat, int cf, const char *s, const char *expect)
{
	regex_t re;
	regmatch_t m[1];
	char got[40];
	int e = regcomp(&re, pat, cf | REG_EXTENDED);
	if (e) strcpy(got, e == REG_ECOLLATE ? "E" : "other error");
	else {
		if (regexec(&re, s, 1, m, 0)) strcpy(got, "-");
		else {
			got[0] = (char)('0' + m[0].rm_so);
			got[1] = ',';
			got[2] = (char)('0' + m[0].rm_eo);
			got[3] = 0;
		}
		regfree(&re);
	}
	if (strcmp(got, expect)) {
		failures++;
		os("FAIL line "); oi(line); os(": /"); os(pat); os("/ \""); os(s); os("\": got "); os(got);
		os(" expected "); os(expect); onl();
	}
}
#define R(p, s, e) r(__LINE__, p, 0, s, e)
#define RI(p, s, e) r(__LINE__, p, REG_ICASE, s, e)
#define F(p, s, f, e) do { if ((fnmatch(p, s, f) == 0) != (e)) { failures++; \
	os("FAIL line "); oi(__LINE__); os(": fnmatch "); os(p); os(" "); os(s); onl(); } } while (0)

static void elements(void)
{
	/* collating symbols */
	R("[[.ch.]]", "xchy", "1,3");
	R("^[[.ch.]]$", "c", "-");
	R("^[[.ch.]]*$", "chch", "0,4");
	R("^[[.ch.]a]+$", "achch", "0,5");
	R("^[[.ch.]c]h$", "ch", "0,2");       /* 'c' then 'h' */
	R("^[[.ch.]c]h$", "chh", "0,3");      /* "ch" then 'h' */
	R("[[.xy.]]", "xy", "E");
	R("[[.c.]]", "c", "0,1");
	RI("^[[.ch.]]$", "CH", "0,2");
	/* equivalence classes: "ch" has a first-level weight of its own */
	R("^[[=ch=]]$", "ch", "0,2");
	R("^[[=ch=]]$", "c", "-");
	R("^[[=c=]]$", "ch", "-");
	R("^[[=ch=]a]+$", "chach", "0,5");
	/* ranges, by collation sequence (c < ch < d): characters only */
	R("^[a-z]$", "ch", "-");
	R("^[a-z]+$", "ch", "0,2");
	R("^[c-d]$", "ch", "-");
	R("^[a-c]$", "ch", "-");
	R("^[[.ch.]-d]$", "c", "-");
	R("^[[.ch.]-d]$", "ch", "-");
	R("^[[.ch.]-d]$", "d", "0,1");
	R("^[a-[.ch.]]$", "c", "0,1");
	R("^[[.ch.]-[.ch.]]$", "ch", "-");
	R("[d-[.ch.]]", "d", "other error");   /* REG_ERANGE */
	/* non-matching lists: single characters */
	R("^[^a]$", "ch", "-");
	R("^[^a]*$", "ch", "0,2");
	R("^x[^a]y$", "xchy", "-");
	R("^[^[.ch.]]$", "ch", "-");
	R("^[^[.ch.]]$", "c", "0,1");
	R("^[^c-d]$", "ch", "-");
	R("^[^[=ch=]]$", "ch", "-");

	F("[[.ch.]]", "ch", 0, 1);
	F("[[.ch.]]", "c", 0, 0);
	F("[[.ch.]c]h", "ch", 0, 1);
	F("[[.ch.]c]h", "chh", 0, 1);
	F("*[[.ch.]]", "ach", 0, 1);
	F("[[.ch.]][[.ch.]]", "chch", 0, 1);
	F("[[=ch=]]", "ch", 0, 1);
	F("[c-d]", "ch", 0, 0);
	F("[a-c]", "ch", 0, 0);
	F("[!a]", "ch", 0, 0);
	F("[![.ch.]]", "ch", 0, 0);
	F("x[!a]y", "xchy", 0, 0);
	F("[[.ch.]]", "CH", FNM_CASEFOLD, 1);
	F("[[.xy.]]", "x", 0, 0);              /* not valid: '[' is itself */
	F("[[.xy.]]", "[[.xy.]]", 0, 0);
}

int main(void)
{
	static const char *const names[] = { "xx_XX.UTF-8", "xx_XX.ISO-8859-1" };
	int i, n = 0;
	/* the C locale has no multi-character elements */
	R("[[.ch.]]", "ch", "E");
	R("[[=ch=]]", "ch", "E");
	F("[[.ch.]]", "ch", 0, 0);
	F("[[.ch.]]", "[.ch.]]", 0, 0);
	for (i = 0; i < 2; i++)
		if (setlocale(LC_ALL, names[i])) {
			elements();
			n++;
		}
	setlocale(LC_ALL, "C");
	os(failures ? "failures\n" : "ok\n");
	if (!n) os("(no test locales: LOCPATH)\n");
	oflush();
	return failures != 0;
}

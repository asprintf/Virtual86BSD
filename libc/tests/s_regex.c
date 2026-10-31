/* Self-checking: POSIX rules for subexpressions (glibc departs from
 * them, so these cannot be differential), minimal repetition (POSIX.1-2024),
 * REG_STARTEND, REG_NOSUB, back-references, long subjects. */
#define _XOPEN_SOURCE 800
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"

/* expect: "(so,eo)(so,eo)..." for every subexpression, or "nomatch" */
static void t(int line, const char *pat, int cf, const char *s, const char *expect)
{
	regex_t re;
	regmatch_t m[10];
	char got[200], *g = got;
	int e = regcomp(&re, pat, cf), i;
	if (e) { failures++; os("FAIL line "); oi(line); os(": regcomp "); os(pat); onl(); return; }
	if (regexec(&re, s, 10, m, 0)) strcpy(got, "nomatch");
	else {
		for (i = 0; i <= (int)re.re_nsub && i < 10; i++) {
			long v[2] = { (long)m[i].rm_so, (long)m[i].rm_eo };
			int k;
			*g++ = '(';
			for (k = 0; k < 2; k++) {
				char tmp[24], *q = tmp + sizeof tmp;
				unsigned long u = v[k] < 0 ? (unsigned long)-v[k] : (unsigned long)v[k];
				*--q = 0;
				do *--q = (char)('0' + u % 10); while (u /= 10);
				if (v[k] < 0) *--q = '-';
				strcpy(g, q); g += strlen(q);
				*g++ = k ? ')' : ',';
			}
		}
		*g = 0;
	}
	if (strcmp(got, expect)) {
		failures++;
		os("FAIL line "); oi(line); os(": /"); os(pat); os("/ \""); os(s); os("\": got "); os(got);
		os(" expected "); os(expect); onl();
	}
	regfree(&re);
}
#define T(p, c, s, e) t(__LINE__, p, c, s, e)
#define E REG_EXTENDED

int main(void)
{
	regex_t re;
	regmatch_t m[4];
	char *big;
	int i;

	/* the whole match leftmost-longest, then each subpattern longest */
	T("(a|ab)(c|bcd)(d*)", E, "abcd", "(0,4)(0,2)(2,3)(3,4)");
	T("(a|ab)(bc|c)", E, "abcd", "(0,3)(0,2)(2,3)");
	T("(wee|week)(knights|night)", E, "weeknights", "(0,10)(0,3)(3,10)");
	T("(.*)c", E, "abcabc", "(0,6)(0,5)");
	T("a*(a*)", E, "aa", "(0,2)(2,2)");
	T("(a*)(a*)", E, "aa", "(0,2)(0,2)(2,2)");
	T("x(a|ab)(c|bcd)y?", E, "xabcd", "(0,5)(1,2)(2,5)");
	T("(.|(.+|b)).+", E, "bac", "(0,3)(0,2)(0,2)");
	T("((b?)|.?).*.+", E, "cccbac", "(0,6)(0,1)(-1,-1)");
	/* last iteration; groups inside it that did not take part are -1 */
	T("(a|b)*", E, "ab", "(0,2)(1,2)");
	T("((a)|b)*", E, "ab", "(0,2)(1,2)(-1,-1)");
	T("((a)|(b))+", E, "ab", "(0,2)(1,2)(-1,-1)(1,2)");
	T("(b|(.))+", E, "bbaccb", "(0,6)(5,6)(-1,-1)");
	T("(.)*", E, "abc", "(0,3)(2,3)");
	T("(a{2})*", E, "aaaaa", "(0,4)(2,4)");
	T("(..)*(...)*", E, "a", "(0,0)(-1,-1)(-1,-1)");
	T("(..)*(...)*", E, "abcd", "(0,4)(2,4)(-1,-1)");
	T("(ab)*(c)", E, "ababc", "(0,5)(2,4)(4,5)");
	T("(a|b)?c", E, "c", "(0,1)(-1,-1)");
	/* an empty body takes part once */
	T("(a*)*", E, "b", "(0,0)(0,0)");
	T("(a*)+", E, "b", "(0,0)(0,0)");
	T("\\(a*\\)*", 0, "b", "(0,0)(0,0)");
	T("()", E, "x", "(0,0)(0,0)");
	/* minimal repetition */
	T(".*?c", E, "abc abc", "(0,3)");
	T(".*c", E, "abc abc", "(0,7)");
	T("(a+?)(a*)", E, "aaa", "(0,3)(0,1)(1,3)");
	T("a{2,4}?", E, "aaaaa", "(0,2)");
	T("(a?\?)(a*)", E, "aa", "(0,2)(0,0)(0,2)");
	T(".*c", E | REG_MINIMAL, "abc abc", "(0,3)");
	T(".*?c", E | REG_MINIMAL, "abc abc", "(0,7)");
	T("<.+?>", E, "<a><b>", "(0,3)");
	/* back-references */
	T("\\(a\\)\\1", 0, "xaa", "(1,3)(1,2)");
	T("\\(.*\\)\\1", 0, "abcabc", "(0,6)(0,3)");
	T("\\(a*\\)\\(b*\\)\\1\\2", 0, "aabaab", "(0,6)(0,2)(2,3)");
	T("\\([ab]\\([ab]*\\)\\(a*\\)a*a\\)\\2", 0, "bbaabbaa", "(0,5)(0,4)(1,2)(2,3)");
	T("\\(b\\)\\{0,2\\}\\1", 0, "bbcb", "(0,2)(0,1)");
	T("\\(a\\|b\\)*c\\1", 0, "abacb", "nomatch");
	T("\\(a\\|b\\)*c\\1", 0, "abaca", "(0,5)(2,3)");
	T("(a*)b\\1", E, "aabaa", "(0,5)(0,2)");
	T("\\(x*\\)y\\1", 0, "xxyx", "(1,4)(1,2)");
	T("\\(A\\)\\1", REG_ICASE, "aA", "(0,2)(0,1)");
	/* anchors */
	T("^a|b", E, "ab", "(0,1)");
	T("(^a)", E, "ba", "nomatch");
	T("a$|b", E, "ab", "(1,2)");
	T("\\(^a\\)", 0, "a", "(0,1)(0,1)");
	T("x$\\|y", 0, "xy", "(1,2)");

	/* REG_NOSUB */
	regcomp(&re, "a(b)c", E | REG_NOSUB);
	m[0].rm_so = 77;
	CHECK(regexec(&re, "xabc", 1, m, 0) == 0 && m[0].rm_so == 77);
	CHECK(regexec(&re, "xab", 1, m, 0) == REG_NOMATCH);
	regfree(&re);
	/* nmatch beyond the subexpressions */
	regcomp(&re, "(a)", E);
	for (i = 0; i < 4; i++) m[i].rm_so = m[i].rm_eo = 99;
	CHECK(regexec(&re, "a", 4, m, 0) == 0 && m[1].rm_eo == 1 && m[2].rm_so == -1 && m[3].rm_eo == -1);
	m[1].rm_so = 99;
	CHECK(regexec(&re, "a", 1, m, 0) == 0 && m[0].rm_eo == 1 && m[1].rm_so == 99);
	regfree(&re);
	/* REG_STARTEND: a null byte inside, offsets from the string */
	regcomp(&re, "b.c", E);
	m[0].rm_so = 2; m[0].rm_eo = 7;
	CHECK(regexec(&re, "abab\0cd", 1, m, REG_STARTEND) == 0 && m[0].rm_so == 3 && m[0].rm_eo == 6);
	m[0].rm_so = 4; m[0].rm_eo = 7;
	CHECK(regexec(&re, "abab\0cd", 1, m, REG_STARTEND) == REG_NOMATCH);
	regfree(&re);
	regcomp(&re, "^b", E);
	m[0].rm_so = 1; m[0].rm_eo = 2;
	CHECK(regexec(&re, "ab", 1, m, REG_STARTEND) == REG_NOMATCH);   /* '^' is at offset 0 only */
	regfree(&re);
	regcomp(&re, "^b", E | REG_NEWLINE);
	m[0].rm_so = 2; m[0].rm_eo = 3;
	CHECK(regexec(&re, "a\nb", 1, m, REG_STARTEND) == 0 && m[0].rm_so == 2);
	regfree(&re);
	/* regerror */
	CHECK(regerror(REG_EPAREN, 0, 0, 0) == strlen("Unmatched ( or \\(") + 1);

	/* long subjects: linear in practice */
	big = malloc(200001);
	memset(big, 'a', 200000);
	big[199999] = 'b';
	big[200000] = 0;
	regcomp(&re, ".*(a)", E);
	CHECK(regexec(&re, big, 2, m, 0) == 0 && m[0].rm_eo == 199999 && m[1].rm_so == 199998);
	regfree(&re);
	regcomp(&re, "(a|b)*b", E);
	CHECK(regexec(&re, big, 2, m, 0) == 0 && m[0].rm_eo == 200000 && m[1].rm_so == 199998);
	regfree(&re);
	regcomp(&re, "((a*)*)*b", E);
	CHECK(regexec(&re, big, 3, m, 0) == 0 && m[0].rm_eo == 200000);
	regfree(&re);
	big[2000] = 0;
	big[1999] = 'b';
	regcomp(&re, "\\(a*\\)\\1b", 0);
	CHECK(regexec(&re, big, 2, m, 0) == 0 && m[0].rm_so == 1 && m[1].rm_eo == 1000);
	regfree(&re);
	free(big);

	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}

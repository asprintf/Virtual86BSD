#ifndef _REGEX_H
#define _REGEX_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

typedef long regoff_t;

typedef struct {
	size_t re_nsub;
	void *__re_prog;
	int __re_flags;
} regex_t;

typedef struct {
	regoff_t rm_so;
	regoff_t rm_eo;
} regmatch_t;

#define REG_EXTENDED 1
#define REG_ICASE    2
#define REG_NEWLINE  4
#define REG_NOSUB    8
#define REG_MINIMAL  16

#define REG_NOTBOL   1
#define REG_NOTEOL   2
/* Extension (BSD, glibc): match in [pmatch[0].rm_so, pmatch[0].rm_eo)
 * of string, which may contain null bytes. Offsets stay relative to
 * string, and so do the anchors: '^' matches at rm_so only if rm_so is 0. */
#define REG_STARTEND 4

#define REG_NOMATCH  1
#define REG_BADPAT   2
#define REG_ECOLLATE 3
#define REG_ECTYPE   4
#define REG_EESCAPE  5
#define REG_ESUBREG  6
#define REG_EBRACK   7
#define REG_EPAREN   8
#define REG_EBRACE   9
#define REG_BADBR    10
#define REG_ERANGE   11
#define REG_ESPACE   12
#define REG_BADRPT   13

__BEGIN_DECLS
int    regcomp(regex_t *__RESTRICT, const char *__RESTRICT, int);
size_t regerror(int, const regex_t *__RESTRICT, char *__RESTRICT, size_t);
int    regexec(const regex_t *__RESTRICT, const char *__RESTRICT, size_t, regmatch_t *__RESTRICT, int);
void   regfree(regex_t *);
__END_DECLS
#endif

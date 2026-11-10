#ifndef _WORDEXP_H
#define _WORDEXP_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

typedef struct {
	size_t we_wordc;
	char **we_wordv;
	size_t we_offs;
} wordexp_t;

#define WRDE_APPEND  0x01
#define WRDE_DOOFFS  0x02
#define WRDE_NOCMD   0x04
#define WRDE_REUSE   0x08
#define WRDE_SHOWERR 0x10
#define WRDE_UNDEF   0x20

#define WRDE_BADCHAR 1
#define WRDE_BADVAL  2
#define WRDE_CMDSUB  3
#define WRDE_NOSPACE 4
#define WRDE_SYNTAX  5

__BEGIN_DECLS
int  wordexp(const char *__RESTRICT, wordexp_t *__RESTRICT, int);
void wordfree(wordexp_t *);
__END_DECLS
#endif

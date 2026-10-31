#ifndef _FNMATCH_H
#define _FNMATCH_H
#include <bits/features.h>

#define FNM_NOMATCH    1
#define FNM_NOESCAPE   0x02
#define FNM_PATHNAME   0x01
#define FNM_PERIOD     0x04
#define FNM_CASEFOLD   0x10
#define FNM_IGNORECASE FNM_CASEFOLD

__BEGIN_DECLS
int fnmatch(const char *, const char *, int);
__END_DECLS
#endif

#ifndef _STRINGS_H
#define _STRINGS_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_locale_t
#include <bits/alltypes.h>

__BEGIN_DECLS
#if __XSI_VISIBLE
int ffs(int);
int ffsl(long);
int ffsll(long long);
#endif
int strcasecmp(const char *, const char *);
int strcasecmp_l(const char *, const char *, locale_t);
int strncasecmp(const char *, const char *, size_t);
int strncasecmp_l(const char *, const char *, size_t, locale_t);
__END_DECLS
#endif

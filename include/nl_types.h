#ifndef _NL_TYPES_H
#define _NL_TYPES_H
#include <bits/features.h>

#define NL_SETD 1
#define NL_CAT_LOCALE 1

typedef void *nl_catd;
typedef int nl_item;

__BEGIN_DECLS
int    catclose(nl_catd);
char  *catgets(nl_catd, int, int, const char *);
nl_catd catopen(const char *, int);
__END_DECLS
#endif

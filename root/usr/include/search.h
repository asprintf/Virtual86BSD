#ifndef _SEARCH_H
#define _SEARCH_H
#include <bits/features.h>
#define __NEED_size_t
#include <bits/alltypes.h>

typedef struct entry {
	char *key;
	void *data;
} ENTRY;

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;

typedef void posix_tnode;

__BEGIN_DECLS
int hcreate(size_t);
void hdestroy(void);
ENTRY *hsearch(ENTRY, ACTION);
void insque(void *, void *);
void *lfind(const void *, const void *, size_t *, size_t, int (*)(const void *, const void *));
void *lsearch(const void *, void *, size_t *, size_t, int (*)(const void *, const void *));
void remque(void *);
void *tdelete(const void *__RESTRICT, posix_tnode **__RESTRICT, int (*)(const void *, const void *));
posix_tnode *tfind(const void *, posix_tnode *const *, int (*)(const void *, const void *));
posix_tnode *tsearch(const void *, posix_tnode **, int (*)(const void *, const void *));
void twalk(const posix_tnode *, void (*)(const posix_tnode *, VISIT, int));
__END_DECLS

#endif

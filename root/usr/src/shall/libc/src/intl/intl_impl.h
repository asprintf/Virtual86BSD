/* Internal: message catalogs. */
#ifndef _INTERNAL_INTL_H
#define _INTERNAL_INTL_H
#include <stddef.h>

/* Try the templates of an NLSPATH-like list (":"-separated, %N %L %l %t
 * %c %%) for name and the locale name loc; f is called with each path
 * and returns non-zero to stop. Returns what f returned last. */
int __nlspath(const char *spec, const char *name, const char *loc, int (*f)(const char *, void *), void *arg);

/* a whole file mapped read-only, or 0 */
void *__map_file(const char *path, size_t *size);
#endif

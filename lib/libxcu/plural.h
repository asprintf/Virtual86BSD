/*
 * plural.h - the plural forms of a messages object: the header's
 * "nplurals=count; plural=expression" and its C expression of n (shared
 * by gettext and msgfmt)
 */

#ifndef PLURAL_H
#define PLURAL_H

#include <stdbool.h>

struct plural;

/* the expression of s (to its end, or to a ';' outside parentheses):
   NULL if it is not a valid expression; *end after it when end */
struct plural *plural_parse(const char *s, const char **end);
/* its value for n: false when it cannot be evaluated (division by zero) */
bool plural_eval(const struct plural *p, unsigned long n, unsigned long *value);
void plural_free(struct plural *p);

/* the "nplurals=count; plural=expression" of a header: 1 found (count and
   the parsed expression set), 0 none, -1 malformed */
int plural_header(const char *header, unsigned long *count, struct plural **expr);

/* the "charset=codeset" of a header, in buf (of size n): false if none */
bool header_charset(const char *header, char *buf, unsigned long n);

#endif

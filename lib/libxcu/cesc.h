/*
 * cesc.h - C-language escape sequences in strings, as in the string
 * literals of ISO C (shared by gettext, msgfmt and xgettext)
 */

#ifndef CESC_H
#define CESC_H

#include <stddef.h>

enum {
    CESC_C = 1,                 /* \c ends the string (and is reported) */
    CESC_STRICT = 2             /* an unknown escape is an error */
};

/* the n bytes of in with their escapes processed, into out (room for n
   bytes; the result is never longer): 0, or 1 when \c ended it (CESC_C),
   or -1 for an unknown escape (CESC_STRICT) or a value too large for a
   byte; *outlen is the length of out (also on an error: so far) */
int cesc(const char *in, size_t n, char *out, size_t *outlen, int flags);

/* s (n bytes) as the inside of a C string literal, escaped as msgfmt and
   xgettext write it (\" \\ \n \t \r \a \b \f \v, other control bytes as
   octal), into out (room for 4n + 1 bytes, NUL-terminated); its length */
size_t cesc_quote(const char *s, size_t n, char *out);

#endif

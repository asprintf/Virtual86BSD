/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

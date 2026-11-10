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

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
 * cesc.c - C-language escape sequences (see cesc.h)
 */

#include "cesc.h"

#include <string.h>

static int digit(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    else
        return -1;
    return v < base ? v : -1;
}

int cesc(const char *in, size_t n, char *out, size_t *outlen, int flags)
{
    size_t i = 0, o = 0;
    int r = 0;
    while (i < n) {
        char c = in[i++];
        if (c != '\\' || i == n) {
            out[o++] = c;
            continue;
        }
        c = in[i++];
        switch (c) {
        case 'a': out[o++] = '\a'; break;
        case 'b': out[o++] = '\b'; break;
        case 'f': out[o++] = '\f'; break;
        case 'n': out[o++] = '\n'; break;
        case 'r': out[o++] = '\r'; break;
        case 't': out[o++] = '\t'; break;
        case 'v': out[o++] = '\v'; break;
        case '\\': case '\'': case '"': case '?': out[o++] = c; break;
        case 'x': {
            unsigned long v = 0;
            int k = 0, d;
            while (i < n && (d = digit((unsigned char)in[i], 16)) >= 0) {
                v = v * 16 + (unsigned long)d;
                if (v > 0xff) {
                    *outlen = o;
                    return -1;
                }
                i++, k++;
            }
            if (k == 0) {
                if (flags & CESC_STRICT) {
                    *outlen = o;
                    return -1;
                }
                out[o++] = '\\';
                out[o++] = 'x';
                break;
            }
            out[o++] = (char)v;
            break;
        }
        case 'c':
            if (flags & CESC_C) {
                *outlen = o;
                return 1;
            }
            /* fall through */
        default:
            if (c >= '0' && c <= '7') {
                unsigned v = (unsigned)(c - '0');
                for (int k = 1; k < 3 && i < n && in[i] >= '0' && in[i] <= '7'; k++)
                    v = v * 8 + (unsigned)(in[i++] - '0');
                if (v > 0xff) {
                    *outlen = o;
                    return -1;
                }
                out[o++] = (char)v;
                break;
            }
            if (flags & CESC_STRICT) {
                *outlen = o;
                return -1;
            }
            out[o++] = '\\';
            out[o++] = c;
            break;
        }
    }
    *outlen = o;
    return r;
}

size_t cesc_quote(const char *s, size_t n, char *out)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        const char *e = NULL;
        switch (c) {
        case '"': e = "\\\""; break;
        case '\\': e = "\\\\"; break;
        case '\n': e = "\\n"; break;
        case '\t': e = "\\t"; break;
        case '\r': e = "\\r"; break;
        case '\a': e = "\\a"; break;
        case '\b': e = "\\b"; break;
        case '\f': e = "\\f"; break;
        case '\v': e = "\\v"; break;
        }
        if (e) {
            memcpy(out + o, e, 2);
            o += 2;
        } else if (c < 0x20 || c == 0x7f) {
            out[o++] = '\\';
            out[o++] = (char)('0' + (c >> 6));
            out[o++] = (char)('0' + ((c >> 3) & 7));
            out[o++] = (char)('0' + (c & 7));
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return o;
}

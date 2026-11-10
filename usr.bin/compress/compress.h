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
 * compress.h - the parts of compress (compress.c, lzw.c, gzip.c)
 */

#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define IOSIZE 65536

/* buffered input from a descriptor */
struct in {
    int fd;
    size_t pos, len;
    bool eof;
    int err;                    /* errno of a failed read, or 0 */
    uintmax_t count;            /* bytes taken so far */
    unsigned char buf[IOSIZE];
};

/* buffered output to a descriptor; errors are kept for the end */
struct out {
    int fd;
    size_t len;
    int err;                    /* errno of a failed write, or 0 */
    uintmax_t count;            /* bytes given so far */
    unsigned char buf[IOSIZE];
};

int in_refill(struct in *in);
/* the next input byte, or -1 at the end (or on an error, in in->err) */
static inline int in_byte(struct in *in)
{
    if (in->pos < in->len) {
        in->count++;
        return in->buf[in->pos++];
    }
    return in_refill(in);
}
void out_bytes(struct out *out, const void *p, size_t n);
void out_flush(struct out *out);

/* lzw.c: the .Z format.  The decompressors are called after the two
   magic bytes, and return NULL, or a message for bad input */
void lzw_compress(struct in *in, struct out *out, int maxbits);
const char *lzw_decompress(struct in *in, struct out *out);

/* gzip.c: the gzip format (RFC 1952) with DEFLATE (RFC 1951) */
void gzip_compress(struct in *in, struct out *out, int level, time_t mtime);
const char *gzip_decompress(struct in *in, struct out *out);

#endif

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

/* Eras and alternative digits of LC_TIME, for strftime() and strptime(). */
#ifndef _ERA_H
#define _ERA_H
#include <stdint.h>
#include "locale_impl.h"

/* LC_TIME elements */
enum { T_ABDAY = 0, T_DAY = 7, T_ABMON = 14, T_MON = 26, T_AM = 38, T_PM = 39, T_DTFMT = 40,
       T_DFMT = 41, T_TFMT = 42, T_TFMTAMPM = 43, T_ERA_DFMT = 46, T_ALTDIGITS = 47,
       T_ERA_DTFMT = 48, T_ERA_TFMT = 49, T_ERA_N = 50, T_ERA_ENTRIES = 51,
       T_ALTMON = 111, T_ABALTMON = 135 };

/* an era (glibc's binary entries): the start and end dates as year - 1900,
 * month, day; dir is +1 when the years count up from start, else -1 */
struct __era { int dir, offset; int32_t start[3], stop[3]; const char *name, *format; };

unsigned __era_count(const struct __lcat *);
void __era_get(const struct __lcat *, unsigned, struct __era *);
int __era_has(const struct __era *, const int32_t date[3]);
/* whether y can be the era year (%Ey) of e */
int __era_year_ok(const struct __era *, long long y);

/* the alternative digits for v in 0..99, or 0 */
const char *__alt_digit(const struct __lcat *, long long v);
/* the value of the longest alternative digits at *s, which it skips; or -1 */
int __alt_digit_scan(const struct __lcat *, const char **s);

#endif

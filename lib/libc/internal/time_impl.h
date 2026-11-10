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

/* Internal: calendar arithmetic and the time-zone database. */
#ifndef _INTERNAL_TIME_IMPL_H
#define _INTERNAL_TIME_IMPL_H
#include <time.h>

/* seconds since the Epoch <-> broken-down UTC; 0 / -1 (EOVERFLOW) */
int  __secs_to_tm(long long t, struct tm *tm);
long long __tm_to_secs(const struct tm *tm);   /* normalises nothing; any field ranges */
long long __days_from_civil(long long y, unsigned m, unsigned d);

/* The current zone (TZ, else /etc/localtime, else UTC).  __tz_lookup
 * gives the offset east of UTC, the DST flag and the abbreviation in
 * effect at UTC time t. */
void __tz_lock(void);
void __tz_unlock(void);
void __tz_update(void);         /* re-read TZ if it changed; call with the lock */
void __tz_lookup(long long t, long *off, int *isdst, const char **abbr);
/* offsets of standard and daylight time near UTC time t (dst == std if none) */
void __tz_offsets(long long t, long *stdoff, long *dstoff);

extern char *__tzname[2];
extern long __timezone;
extern int __daylight;

struct tm *__localtime_r(const time_t *, struct tm *);
struct tm *__gmtime_r(const time_t *, struct tm *);
size_t __strftime_l(char *, size_t, const char *, const struct tm *, void *);
#endif

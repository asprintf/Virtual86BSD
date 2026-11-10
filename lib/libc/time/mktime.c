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

/* mktime: local broken-down time -> time_t.
 *
 * The fields are normalised by converting to seconds as if UTC and
 * subtracting the zone offset in effect, found by iterating
 * t = local - off(t).  If tm_isdst asks for the other kind of time than
 * the one in effect (positive: daylight time, 0: standard time), the
 * time is read with that kind's offset, as other systems do; a negative
 * tm_isdst takes the time as it is.  A time in a gap (skipped by a
 * forward change) is read with the offset before the change.
 */
#include <time.h>
#include <errno.h>
#include "time_impl.h"

time_t mktime(struct tm *tm)
{
	long long local = __tm_to_secs(tm), t;
	long off, off2, stdoff, dstoff;
	int dst, dst2, i;
	const char *abbr;
	struct tm out;

	__tz_lock();
	__tz_update();
	/* first guess: the offset at local time read as UTC */
	__tz_lookup(local, &off, &dst, &abbr);
	t = local - off;
	for (i = 0; i < 4; i++) {
		__tz_lookup(t, &off2, &dst2, &abbr);
		if (off2 == off) break;
		off = off2;
		t = local - off;
	}
	if (i == 4) {
		/* No fixed point: the local time falls in a gap.  Read it with
		 * the offset before the change, which moves it forward. */
		__tz_lookup(local - off, &off2, &dst2, &abbr);
		if (local - off2 > local - off) off = off2;
		t = local - off;
	}
	__tz_lookup(t, &off, &dst, &abbr);
	if (tm->tm_isdst >= 0 && (tm->tm_isdst > 0) != dst) {
		/* the caller asked for the other kind of time: reinterpret */
		__tz_offsets(t, &stdoff, &dstoff);
		if (stdoff != dstoff) t = tm->tm_isdst > 0 ? local - dstoff : local - stdoff;
	}
	__tz_lookup(t, &off, &dst, &abbr);
	__tz_unlock();

	if ((time_t)t != t || __secs_to_tm(t + off, &out)) { errno = EOVERFLOW; return (time_t)-1; }
	out.tm_isdst = dst;
	out.tm_gmtoff = off;
	out.tm_zone = abbr;
	*tm = out;
	return (time_t)t;
}

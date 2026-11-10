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

/* The format of asctime(): "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n". */
#include <time.h>
#include <string.h>

static char *num(char *p, long v, int width, char pad)
{
	char t[24];
	int n = 0, neg = v < 0;
	unsigned long u = neg ? 0UL - (unsigned long)v : (unsigned long)v;
	do t[n++] = (char)('0' + u % 10); while (u /= 10);
	if (neg) t[n++] = '-';
	while (n < width) t[n++] = pad;
	while (n) *p++ = t[--n];
	return p;
}

char *__asctime_r(const struct tm *tm, char *buf)
{
	static const char wday[] = "SunMonTueWedThuFriSat";
	static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	char *p = buf;
	if ((unsigned)tm->tm_wday < 7) memcpy(p, wday + 3 * tm->tm_wday, 3); else memcpy(p, "???", 3);
	p += 3; *p++ = ' ';
	if ((unsigned)tm->tm_mon < 12) memcpy(p, mon + 3 * tm->tm_mon, 3); else memcpy(p, "???", 3);
	p += 3;
	p = num(p, tm->tm_mday, 3, ' '); *p++ = ' ';
	p = num(p, tm->tm_hour, 2, '0'); *p++ = ':';
	p = num(p, tm->tm_min, 2, '0'); *p++ = ':';
	p = num(p, tm->tm_sec, 2, '0'); *p++ = ' ';
	p = num(p, 1900L + tm->tm_year, 0, ' ');
	*p++ = '\n'; *p = 0;
	return buf;
}

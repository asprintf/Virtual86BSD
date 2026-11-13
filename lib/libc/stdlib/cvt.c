/*
 * Copyright (c) 2026, asprintf
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

/* ecvt, fcvt, gcvt: SUSv2, SUSv3 */
/* At most 17 digits; ecvt() gives exactly ndigit even when rounding carries
 * (glibc gives one more). */
#define _DEFAULT_SOURCE
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXDIG DBL_DECIMAL_DIG

static char ebuf[MAXDIG + 2];
static char fbuf[DBL_MAX_10_EXP + 2 + MAXDIG + 2];

static int special(double v, char *buf, int *decpt, int *sign)
{
	*sign = signbit(v) != 0;
	if (isfinite(v))
		return 0;
	strcpy(buf, isnan(v) ? "nan" : "inf");
	*decpt = 0;
	return 1;
}

char *ecvt(double value, int ndigit, int *decpt, int *sign)
{
	char tmp[MAXDIG + 16];
	char *p, *o = ebuf;
	if (special(value, ebuf, decpt, sign))
		return ebuf;
	value = fabs(value);
	if (ndigit > MAXDIG)
		ndigit = MAXDIG;
	if (value == 0) {
		if (ndigit > 0)
			memset(ebuf, '0', ndigit);
		ebuf[ndigit > 0 ? ndigit : 0] = 0;
		*decpt = 1;
		return ebuf;
	}
	snprintf(tmp, sizeof tmp, "%.*e", ndigit > 0 ? ndigit - 1 : MAXDIG - 1, value);
	for (p = tmp; *p != 'e'; p++)
		if (*p >= '0' && *p <= '9' && ndigit > 0)
			*o++ = *p;
	*o = 0;
	*decpt = atoi(p + 1) + 1;
	return ebuf;
}

char *fcvt(double value, int ndigit, int *decpt, int *sign)
{
	char *p, *o = fbuf;
	int intdig = 0, skip = 1;
	if (special(value, fbuf, decpt, sign))
		return fbuf;
	value = fabs(value);
	if (ndigit > MAXDIG)
		ndigit = MAXDIG;
	if (ndigit < 0)
		ndigit = 0;
	snprintf(fbuf, sizeof fbuf, "%.*f", ndigit, value);
	for (p = fbuf; *p; p++) {
		if (*p < '0' || *p > '9') {
			intdig = (int)(p - fbuf);
			continue;
		}
		if (skip && *p == '0' && value != 0)
			continue;
		skip = 0;
		*o++ = *p;
	}
	if (!intdig)
		intdig = (int)(p - fbuf);
	*o = 0;
	if (value == 0)
		*decpt = 1;
	else
		*decpt = intdig - (int)((p - fbuf) - (ndigit ? 1 : 0) - (o - fbuf));
	return fbuf;
}

char *gcvt(double value, int ndigit, char *buf)
{
	if (ndigit > MAXDIG)
		ndigit = MAXDIG;
	snprintf(buf, 32, "%.*g", ndigit, value);
	return buf;
}

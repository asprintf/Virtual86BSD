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

/* The termcap names' lookups for termcap.c, which includes this after
 * termcap_map.c; NetBSD generates them with nbperf(1). */

static uint32_t
_t_find(const TENTRY *t, size_t n, const unsigned char *key, size_t len)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (strncmp(t[i].id, (const char *)key, len) == 0 &&
		    t[i].id[len] == '\0')
			return (uint32_t)i;
	return (uint32_t)n;
}

static uint32_t
_t_flaghash(const unsigned char *key, size_t len)
{
	return _t_find(_ti_cap_flagids, __arraycount(_ti_cap_flagids), key, len);
}

static uint32_t
_t_numhash(const unsigned char *key, size_t len)
{
	return _t_find(_ti_cap_numids, __arraycount(_ti_cap_numids), key, len);
}

static uint32_t
_t_strhash(const unsigned char *key, size_t len)
{
	return _t_find(_ti_cap_strids, __arraycount(_ti_cap_strids), key, len);
}

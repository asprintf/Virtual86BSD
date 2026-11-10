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

#include <unistd.h>
#include <string.h>
#include <errno.h>

size_t confstr(int name, char *buf, size_t len)
{
	const char *s;
	size_t l;
	switch (name) {
	case _CS_PATH: s = "/bin:/usr/bin"; break;
	case _CS_POSIX_V8_LP64_OFF64_CFLAGS: case _CS_POSIX_V7_LP64_OFF64_CFLAGS:
	case _CS_POSIX_V8_LP64_OFF64_LDFLAGS: case _CS_POSIX_V7_LP64_OFF64_LDFLAGS:
	case _CS_POSIX_V8_LP64_OFF64_LIBS: case _CS_POSIX_V7_LP64_OFF64_LIBS:
	/* LPBIG_OFFBIG (int of at least 32 bits; long, off_t and pointers of
	   at least 64) is the same environment */
	case _CS_POSIX_V8_LPBIG_OFFBIG_CFLAGS: case _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS:
	case _CS_POSIX_V8_LPBIG_OFFBIG_LDFLAGS: case _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS:
	case _CS_POSIX_V8_LPBIG_OFFBIG_LIBS: case _CS_POSIX_V7_LPBIG_OFFBIG_LIBS:
	case _CS_POSIX_V8_THREADS_CFLAGS: case _CS_POSIX_V7_THREADS_CFLAGS:
	case _CS_POSIX_V8_THREADS_LDFLAGS: case _CS_POSIX_V7_THREADS_LDFLAGS:
	case _CS_V8_ENV: case _CS_V7_ENV:
		s = ""; break;
	case _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS:
		s = "POSIX_V8_LP64_OFF64\nPOSIX_V8_LPBIG_OFFBIG"; break;
	case _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS:
		s = "POSIX_V7_LP64_OFF64\nPOSIX_V7_LPBIG_OFFBIG"; break;
	default:
		if ((unsigned)name > _CS_V7_ENV) { errno = EINVAL; return 0; }
		/* the unsupported programming environments: no value */
		s = ""; break;
	}
	l = strlen(s);
	if (len) {
		size_t k = l < len ? l : len - 1;
		memcpy(buf, s, k);
		buf[k] = 0;
	}
	return l + 1;
}

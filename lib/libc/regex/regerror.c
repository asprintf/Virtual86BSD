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

#include <regex.h>
#include <string.h>

/* The messages of glibc, which scripts and tests tend to expect. */
static const char *const msgs[] = {
	"Success",
	"No match",
	"Invalid regular expression",
	"Invalid collation character",
	"Invalid character class name",
	"Trailing backslash",
	"Invalid back reference",
	"Unmatched [, [^, [:, [., or [=",
	"Unmatched ( or \\(",
	"Unmatched \\{",
	"Invalid content of \\{\\}",
	"Invalid range end",
	"Memory exhausted",
	"Invalid preceding regular expression",
};

size_t regerror(int e, const regex_t *__RESTRICT re, char *__RESTRICT buf, size_t size)
{
	const char *m = e >= 0 && e < (int)(sizeof msgs / sizeof *msgs) ? msgs[e] : "Unknown error";
	size_t l = strlen(m) + 1;
	(void)re;
	if (size) {
		size_t n = l < size ? l : size;
		memcpy(buf, m, n - 1);
		buf[n - 1] = 0;
	}
	return l;
}

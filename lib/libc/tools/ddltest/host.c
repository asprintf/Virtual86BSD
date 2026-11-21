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

/* Runs the functions of math/__ddl.c on the host.  stdin: "FUNC a b c d"
 * (hex floats); stdout: "FUNC a b c d OK RE IM" (OK 0: outside the range
 * the function handles). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long double ld;

int __ddl_cexpl(ld, ld, ld *, ld *);
int __ddl_csinhl(ld, ld, ld *, ld *);
int __ddl_ccoshl(ld, ld, ld *, ld *);
int __ddl_ctanhl(ld, ld, ld *, ld *);
int __ddl_catanhl(ld, ld, ld *, ld *);
int __ddl_casinhl(ld, ld, ld *, ld *);
int __ddl_cacoshl(ld, ld, ld *, ld *);
int __ddl_cpowl(ld, ld, ld, ld, ld *, ld *);
int __ddl_tgammal(ld, ld *);
ld __ddl_sinhl(ld);
ld __ddl_coshl(ld);
ld __ddl_asinhl(ld);

ld __x87_rint(ld x) { return rintl(x); }

int main(void)
{
	char f[16], sa[64], sb[64], sc[64], sd[64];
	while (scanf("%15s %63s %63s %63s %63s", f, sa, sb, sc, sd) == 5) {
		ld a = strtold(sa, 0), b = strtold(sb, 0), c = strtold(sc, 0), d = strtold(sd, 0), re = 0, im = 0;
		int ok = 1;
		if (!strcmp(f, "cexp")) ok = __ddl_cexpl(a, b, &re, &im);
		else if (!strcmp(f, "csinh")) ok = __ddl_csinhl(a, b, &re, &im);
		else if (!strcmp(f, "ccosh")) ok = __ddl_ccoshl(a, b, &re, &im);
		else if (!strcmp(f, "ctanh")) ok = __ddl_ctanhl(a, b, &re, &im);
		else if (!strcmp(f, "catanh")) ok = __ddl_catanhl(a, b, &re, &im);
		else if (!strcmp(f, "casinh")) ok = __ddl_casinhl(a, b, &re, &im);
		else if (!strcmp(f, "cacosh")) ok = __ddl_cacoshl(a, b, &re, &im);
		else if (!strcmp(f, "cpow")) ok = __ddl_cpowl(a, b, c, d, &re, &im);
		else if (!strcmp(f, "tgamma")) ok = __ddl_tgammal(a, &re);
		else if (!strcmp(f, "sinh")) re = __ddl_sinhl(a);
		else if (!strcmp(f, "cosh")) re = __ddl_coshl(a);
		else if (!strcmp(f, "asinh")) re = __ddl_asinhl(a);
		else ok = 0;
		printf("%s %s %s %s %s %d %.20La %.20La\n", f, sa, sb, sc, sd, ok, re, im);
	}
	return 0;
}

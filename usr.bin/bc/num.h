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
 * num.h - arbitrary-precision decimal numbers for bc.
 *
 * A number is a sign, a scale (the number of decimal digits after the
 * point) and an integer mantissa M in base 10^9, least significant limb
 * first: the value is (-1)^neg * M / 10^scale.  The mantissa never has
 * leading zero limbs; zero has no limbs and is never negative.
 */
#ifndef NUM_H
#define NUM_H

#include <stddef.h>
#include <stdint.h>

typedef struct Num {
    int neg;
    long scale;
    size_t n;
    uint32_t *d;
} Num;

#define NUM_BASE 1000000000u

void num_init(void);
Num num_zero(void);
Num num_copy(Num a);
void num_free(Num *a);
Num num_from_long(long v);
int num_is_zero(Num a);
int num_cmp(Num a, Num b);              /* <0, 0, >0 */
int num_cmp_abs(Num a, Num b);
Num num_neg(Num a);                     /* a new number */
Num num_rescale(Num a, long scale);     /* truncate or extend */
Num num_add(Num a, Num b);
Num num_sub(Num a, Num b);
Num num_mul(Num a, Num b, long scale);  /* scale min(a+b, max(scale, a, b)) */
Num num_mul_exact(Num a, Num b);        /* scale a+b */
int num_div(Num a, Num b, long scale, Num *q);     /* 0, or -1 when b is 0 */
int num_mod(Num a, Num b, long scale, Num *r);
int num_pow(Num a, Num b, long scale, Num *r);     /* -1 b == 0 for 0^-n; -2 exponent too large */
int num_sqrt(Num a, long scale, Num *r);           /* -1 for a negative number */
extern int gnu_sqrt_quirk;                         /* testing: GNU bc's scale for 0 and 1 */
long num_length(Num a);
int num_to_long(Num a, long *v);        /* the integer part; 0 if it does not fit */
int num_is_integer(Num a);
Num num_trunc(Num a);                   /* the integer part */

/* a constant in base 2..16 (any digits up to F); *digits has no sign */
Num num_parse(const char *s, int base);

/* output: the number in obase, with the line length (0 for none) */
typedef void (*Putc)(int c, void *arg);
void num_print(Num a, long obase, Putc out, void *arg);

#endif

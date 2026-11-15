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

/* lib.c - what the loader needs of a C library: memory and string
   functions, printf, the CRC-32 of a GPT and the 64-bit division that
   gcc calls for in 32-bit code. */
#include "boot.h"

void *memcpy(void *d, const void *s, size_t n) {
	void *r = d;
	__asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
	return r;
}

void *memmove(void *d, const void *s, size_t n) {
	unsigned char *dp = d;
	const unsigned char *sp = s;
	if (dp < sp)
		while (n--)
			*dp++ = *sp++;
	else
		while (n--)
			dp[n] = sp[n];
	return d;
}

void *memset(void *d, int c, size_t n) {
	void *r = d;
	__asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
	return r;
}

int memcmp(const void *a, const void *b, size_t n) {
	const unsigned char *x = a, *y = b;
	for (; n; n--, x++, y++)
		if (*x != *y)
			return *x - *y;
	return 0;
}

size_t strlen(const char *s) {
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

int strcmp(const char *a, const char *b) {
	while (*a && *a == *b)
		a++, b++;
	return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (unsigned char)*a - (unsigned char)*b;
		if (!*a)
			break;
	}
	return 0;
}

char *strchr(const char *s, int c) {
	for (;; s++) {
		if (*s == (char)c)
			return (char *)s;
		if (!*s)
			return NULL;
	}
}

uint32_t crc32(const void *buf, size_t n) {
	static uint32_t table[256];
	if (!table[1])
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t c = i;
			for (int k = 0; k < 8; k++)
				c = c & 1 ? 0xedb88320U ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
	const uint8_t *p = buf;
	uint32_t c = 0xffffffffU;
	while (n--)
		c = table[(c ^ *p++) & 255] ^ (c >> 8);
	return ~c;
}

/* ---- printf: %c %s %d %i %u %x %X %o %p, 0 - and a width, l ignored */
struct out {
	char *buf;
	size_t n, len;
};

static void emit(struct out *o, int c) {
	if (!o->buf)
		putchar(c);
	else if (o->len + 1 < o->n)
		o->buf[o->len] = (char)c;
	o->len++;
}

static void vfmt(struct out *o, const char *f, va_list ap) {
	for (; *f; f++) {
		if (*f != '%') {
			emit(o, *f);
			continue;
		}
		int zero = 0, left = 0, width = 0;
		for (f++; *f == '0' || *f == '-'; f++)
			if (*f == '0')
				zero = 1;
			else
				left = 1;
		if (*f == '*')
			width = va_arg(ap, int), f++;
		else
			for (; *f >= '0' && *f <= '9'; f++)
				width = width * 10 + *f - '0';
		while (*f == 'l')
			f++;
		char tmp[16];
		const char *s = tmp;
		int n = 0, neg = 0;
		uint32_t v;
		unsigned base = 10;
		switch (*f) {
		case 'c':
			tmp[0] = (char)va_arg(ap, int);
			n = 1;
			break;
		case 's':
			s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			n = (int)strlen(s);
			break;
		case 'd':
		case 'i':
			v = (uint32_t)va_arg(ap, int);
			if ((int32_t)v < 0)
				neg = 1, v = -v;
			goto number;
		case 'o':
			base = 8;
			goto unsig;
		case 'x':
		case 'X':
		case 'p':
			base = 16;
			goto unsig;
		case 'u':
		unsig:
			v = va_arg(ap, uint32_t);
		number:
			do {
				unsigned d = v % base;
				tmp[sizeof(tmp) - 1 - n++] = (char)(d < 10 ? '0' + d : (*f == 'X' ? 'A' : 'a') + d - 10);
				v /= base;
			} while (v);
			if (neg)
				tmp[sizeof(tmp) - 1 - n++] = '-';
			s = tmp + sizeof(tmp) - n;
			break;
		case '%':
			tmp[0] = '%';
			n = 1;
			break;
		case 0:
			return;
		default:
			tmp[0] = *f;
			n = 1;
		}
		int pad = width > n ? width - n : 0;
		if (!left && !zero)
			while (pad-- > 0)
				emit(o, ' ');
		if (zero && !left) {
			if (neg && n) {
				emit(o, '-');
				s++, n--;
			}
			while (pad-- > 0)
				emit(o, '0');
		}
		for (int i = 0; i < n; i++)
			emit(o, s[i]);
		if (left)
			while (pad-- > 0)
				emit(o, ' ');
	}
}

int printf(const char *f, ...) {
	struct out o = {NULL, 0, 0};
	va_list ap;
	va_start(ap, f);
	vfmt(&o, f, ap);
	va_end(ap);
	return (int)o.len;
}

int snprintf(char *buf, size_t n, const char *f, ...) {
	struct out o = {buf, n, 0};
	va_list ap;
	va_start(ap, f);
	vfmt(&o, f, ap);
	va_end(ap);
	if (n)
		buf[o.len < n ? o.len : n - 1] = 0;
	return (int)o.len;
}

/* ---- 64-bit division: gcc calls these in 32-bit code */
static uint64_t udivmod(uint64_t n, uint64_t d, uint64_t *rem) {
	uint64_t q = 0, r = 0;
	for (int i = 63; i >= 0; i--) {
		r = r << 1 | (n >> i & 1);
		if (r >= d) {
			r -= d;
			q |= 1ULL << i;
		}
	}
	if (rem)
		*rem = r;
	return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) { return udivmod(n, d, NULL); }

uint64_t __umoddi3(uint64_t n, uint64_t d) {
	uint64_t r;
	udivmod(n, d, &r);
	return r;
}

int64_t __divdi3(int64_t n, int64_t d) {
	int neg = (n < 0) != (d < 0);
	uint64_t q = udivmod(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, NULL);
	return neg ? -(int64_t)q : (int64_t)q;
}

int64_t __moddi3(int64_t n, int64_t d) {
	uint64_t r;
	udivmod(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
	return n < 0 ? -(int64_t)r : (int64_t)r;
}

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

/* <sys/cdefs.h>: the compiler-attribute and declaration macros that BSD
 * sources expect (the names FreeBSD, NetBSD and OpenBSD share, and the few of
 * NetBSD's that its programs use).  Written for V86BSD; the C library's own
 * headers need none of it (they use <bits/features.h>).
 *
 * The attributes are spelled with __attribute__ whatever the compiler: gcc,
 * clang and c17 all take it (c17 does not define __GNUC__, so that is not
 * tested; an attribute it does not know it skips).
 */
#ifndef _SYS_CDEFS_H_
#define _SYS_CDEFS_H_

#include <stdint.h>
#include <bits/features.h>	/* __BEGIN_DECLS, __END_DECLS, __RESTRICT, __NORETURN */

/* token pasting and stringizing */
#define __CONCAT1(x, y)	x ## y
#define __CONCAT(x, y)	__CONCAT1(x, y)
#define __STRING(x)	#x
#define __XSTRING(x)	__STRING(x)

/* old-style prototype wrapper (the prototypes are always there) */
#define __P(protos)	protos

/* functions that do not return, and that have no side effects */
#define __dead		__attribute__((__noreturn__))
#define __dead2		__attribute__((__noreturn__))
#define __pure		__attribute__((__pure__))
#define __const		__attribute__((__const__))

/* code generation and linkage */
#define __always_inline	__attribute__((__always_inline__))
#define __noinline	__attribute__((__noinline__))
#define __used		__attribute__((__used__))
#define __unused	__attribute__((__unused__))
#define __weak		__attribute__((__weak__))
#define __packed	__attribute__((__packed__))
#define __aligned(x)	__attribute__((__aligned__(x)))
#define __section(x)	__attribute__((__section__(x)))
#define __alias(x)	__attribute__((__alias__(x)))
#define __nonnull(x)	__attribute__((__nonnull__ x))
#define __warn_unused_result	__attribute__((__warn_unused_result__))

/* printf-like functions: the format is argument f, the first value argument a */
#define __printflike(f, a)	__attribute__((__format__(__printf__, f, a)))
#define __scanflike(f, a)	__attribute__((__format__(__scanf__, f, a)))

/* branch prediction */
#define __predict_true(exp)	__builtin_expect((exp) != 0, 1)
#define __predict_false(exp)	__builtin_expect((exp) != 0, 0)

/* casting away a qualifier on purpose */
#define __DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#define __UNCONST(a)		__DECONST(void *, a)
#define __DEVOLATILE(type, var)	((type)(uintptr_t)(volatile void *)(var))
#define __USE(a)		((void)(a))

/* the number of elements of an array (NetBSD) */
#define __arraycount(a)		(sizeof(a) / sizeof((a)[0]))

/* identification strings: not kept in the objects; a declaration, as in
 * FreeBSD, for the ';' that follows them */
#define __RCSID(s)		struct __hack
#define __KERNEL_RCSID(n, s)	struct __hack
#define __COPYRIGHT(s)		struct __hack
#define __SCCSID(s)		struct __hack

#endif

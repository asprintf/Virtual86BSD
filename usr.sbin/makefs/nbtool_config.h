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
/*
 * V86BSD: what the NetBSD sources of makefs expect from NetBSD's own libc and
 * headers (NetBSD names it nbtool_config.h, for building makefs on other
 * systems): the fields this system's struct stat lacks are not there, and the
 * few NetBSD library functions the code calls are in misc.c.
 */
#ifndef _NBTOOL_CONFIG_H
#define _NBTOOL_CONFIG_H

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef int64_t quad_t;
typedef uint64_t u_quad_t;

#define HAVE_STRUCT_STAT_ST_FLAGS 0
#define HAVE_STRUCT_STAT_ST_GEN 0
#define HAVE_STRUCT_STAT_ST_MTIMENSEC 0
#define HAVE_STRUCT_STATVFS_F_IOSIZE 0
#define HAVE_STRUCT_STAT_BIRTHTIME 0
#define HAVE_FSTATVFS 0

#ifndef MAXPHYS
#define MAXPHYS		(64 * 1024)
#endif
#ifndef timersub
#define timersub(a, b, r) do { \
	(r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((r)->tv_usec < 0) { (r)->tv_sec--; (r)->tv_usec += 1000000; } \
} while (0)
#endif

/* NetBSD's <util.h> and libc, in misc.c */
void	*emalloc(size_t);
void	*ecalloc(size_t, size_t);
void	*erealloc(void *, size_t);
char	*estrdup(const char *);
long long strsuftoll(const char *, const char *, long long, long long);
char	*strsep(char **, const char *);
const char *inotype(mode_t);

#endif

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

#ifndef _LIMITS_H
#define _LIMITS_H
#include <bits/features.h>

/* ---- Numerical limits (ISO C + CX) ---- */
#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#ifdef __CHAR_UNSIGNED__
# define CHAR_MIN  0
# define CHAR_MAX  UCHAR_MAX
#else
# define CHAR_MIN  SCHAR_MIN
# define CHAR_MAX  SCHAR_MAX
#endif
#define SHRT_MIN   (-1 - 0x7fff)
#define SHRT_MAX   0x7fff
#define USHRT_MAX  0xffff
#define INT_MIN    (-1 - 0x7fffffff)
#define INT_MAX    0x7fffffff
#define UINT_MAX   0xffffffffU
#define LONG_MIN   (-LONG_MAX - 1)
#define LONG_MAX   0x7fffffffffffffffL
#define ULONG_MAX  0xffffffffffffffffUL
#define LLONG_MIN  (-LLONG_MAX - 1)
#define LLONG_MAX  0x7fffffffffffffffLL
#define ULLONG_MAX 0xffffffffffffffffULL
#define MB_LEN_MAX 4

#if __POSIX_VISIBLE
#define SSIZE_MAX  LONG_MAX
#define LONG_BIT   64
#define WORD_BIT   32

/* ---- Runtime invariant values ---- */
/* Omitted (indeterminate; use sysconf()): AIO_*, ARG_MAX, CHILD_MAX,
 * OPEN_MAX, PTHREAD_THREADS_MAX, SEM_NSEMS_MAX, STREAM_MAX, TIMER_MAX,
 * MQ_OPEN_MAX. */
#define AIO_PRIO_DELTA_MAX  20
#define ATEXIT_MAX          2147483647
#define DELAYTIMER_MAX      0x7fffffff
#define HOST_NAME_MAX       255
#define LOGIN_NAME_MAX      256
#define MQ_PRIO_MAX         32768
#define PAGESIZE            4096
#define PTHREAD_DESTRUCTOR_ITERATIONS 4
#define PTHREAD_KEYS_MAX    1024
#define PTHREAD_STACK_MIN   16384
#define RTSIG_MAX           32
#define SEM_VALUE_MAX       0x7fffffff
#define SIGQUEUE_MAX        32
#define SYMLOOP_MAX         40
#define TTY_NAME_MAX        32
#define TZNAME_MAX          6
#if __XSI_VISIBLE
#define IOV_MAX             1024
#define PAGE_SIZE           PAGESIZE
#endif

/* ---- Pathname variable values ---- */
/* Omitted (vary per file system; use pathconf()): FILESIZEBITS, LINK_MAX,
 * SYMLINK_MAX, POSIX_ALLOC_SIZE_MIN, POSIX_REC_*. */
#define MAX_CANON  255
#define MAX_INPUT  255
#define NAME_MAX   255
#define PATH_MAX   4096
#define PIPE_BUF   4096
#define TEXTDOMAIN_MAX 252

/* ---- Runtime increasable values ---- */
#define BC_BASE_MAX        99
#define BC_DIM_MAX         2048
#define BC_SCALE_MAX       99
#define BC_STRING_MAX      1000
#define CHARCLASS_NAME_MAX 14
#define COLL_WEIGHTS_MAX   2
#define EXPR_NEST_MAX      32
#define LINE_MAX           2048
#define NGROUPS_MAX        65536
#define RE_DUP_MAX         255

/* ---- Maximum values ---- */
#define _POSIX_CLOCKRES_MIN 20000000

/* ---- Minimum values ---- */
#define _POSIX_AIO_LISTIO_MAX   2
#define _POSIX_AIO_MAX          1
#define _POSIX_ARG_MAX          4096
#define _POSIX_CHILD_MAX        25
#define _POSIX_DELAYTIMER_MAX   32
#define _POSIX_HOST_NAME_MAX    255
#define _POSIX_LINK_MAX         8
#define _POSIX_LOGIN_NAME_MAX   9
#define _POSIX_MAX_CANON        255
#define _POSIX_MAX_INPUT        255
#define _POSIX_MQ_OPEN_MAX      8
#define _POSIX_MQ_PRIO_MAX      32
#define _POSIX_NAME_MAX         14
#define _POSIX_NGROUPS_MAX      8
#define _POSIX_OPEN_MAX         20
#define _POSIX_PATH_MAX         256
#define _POSIX_PIPE_BUF         512
#define _POSIX_RE_DUP_MAX       255
#define _POSIX_RTSIG_MAX        8
#define _POSIX_SEM_NSEMS_MAX    256
#define _POSIX_SEM_VALUE_MAX    32767
#define _POSIX_SIGQUEUE_MAX     32
#define _POSIX_SSIZE_MAX        32767
#define _POSIX_SS_REPL_MAX      4
#define _POSIX_STREAM_MAX       8
#define _POSIX_SYMLINK_MAX      255
#define _POSIX_SYMLOOP_MAX      8
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4
#define _POSIX_THREAD_KEYS_MAX  128
#define _POSIX_THREAD_THREADS_MAX 64
#define _POSIX_TIMER_MAX        32
#define _POSIX_TTY_NAME_MAX     9
#define _POSIX_TZNAME_MAX       6
#define _POSIX2_BC_BASE_MAX     99
#define _POSIX2_BC_DIM_MAX      2048
#define _POSIX2_BC_SCALE_MAX    99
#define _POSIX2_BC_STRING_MAX   1000
#define _POSIX2_CHARCLASS_NAME_MAX 14
#define _POSIX2_COLL_WEIGHTS_MAX 2
#define _POSIX2_EXPR_NEST_MAX   32
#define _POSIX2_LINE_MAX        2048
#define _POSIX2_RE_DUP_MAX      255
#if __XSI_VISIBLE
#define _XOPEN_IOV_MAX          16
#define _XOPEN_NAME_MAX         255
#define _XOPEN_PATH_MAX         1024
#endif

/* ---- Other invariant values ---- */
#define GETENTROPY_MAX 256
#define NL_ARGMAX      32
#define NL_MSGMAX      32767
#define NL_SETMAX      255
#define NL_TEXTMAX     2048
#define NSIG_MAX       65
#if __XSI_VISIBLE
#define NL_LANGMAX     32
#define NZERO          20
#endif

/* ---- Of older editions (share/doc/legacy.md) ---- */
#if __XSI_OB
/* SUSv2 to SUSv4; the same tokens as c17's <float.h> */
#ifndef DBL_DIG
#define DBL_DIG        15
#endif
#ifndef DBL_MAX
#define DBL_MAX        0x1.fffffffffffffp+1023
#endif
#ifndef FLT_DIG
#define FLT_DIG        6
#endif
#ifndef FLT_MAX
#define FLT_MAX        0x1.fffffep+127F
#endif
#endif
#if __XSI_OB6
#define NL_NMAX        255      /* a collating element's bytes: a length byte */
#endif
#if __XSI_OB5
#define PASS_MAX       128      /* what getpass() keeps */
#ifndef TMP_MAX
#define TMP_MAX        308915776 /* as <stdio.h> */
#endif
#endif
#endif /* __POSIX_VISIBLE */

#endif

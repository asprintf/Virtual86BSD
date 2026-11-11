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

/* Internal: feature-test macro handling (XSH 2.2 "The Compilation Environment").
 *
 * Computes two internal levels that every public header tests:
 *   __POSIX_VISIBLE  0, or the effective _POSIX_C_SOURCE value
 *   __XSI_VISIBLE    0, or the effective _XOPEN_SOURCE value
 * and __C11_VISIBLE for interfaces added in ISO C11/C17.
 *
 * Rules:
 *   - _XOPEN_SOURCE >= 800 implies _POSIX_C_SOURCE 202405L,
 *     700 implies 200809L, 600 implies 200112L, 500 implies 199506L.
 *   - With no feature-test macro at all, everything (POSIX + XSI) is
 *     visible, unless the compiler is in a strict ISO mode
 *     (__STRICT_ANSI__, e.g. gcc -std=c17), in which case only ISO C is.
 *   - _DEFAULT_SOURCE, _BSD_SOURCE and _GNU_SOURCE request everything.
 * __EXT_VISIBLE is 1 in that "everything" case: the few non-POSIX
 * interfaces the library offers (e.g. setgroups) are declared only then.
 * The application's own macros are never defined or altered here.
 */
#ifndef _BITS_FEATURES_H
#define _BITS_FEATURES_H

#if defined(_GNU_SOURCE) || defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || \
    (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) && !defined(_POSIX_SOURCE) && \
     !defined(__STRICT_ANSI__))
#define __POSIX_VISIBLE 202405L
#define __XSI_VISIBLE 800
#define __EXT_VISIBLE 1
#else
#define __EXT_VISIBLE 0
# if defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE - 0) >= 800
#  define __XSI_VISIBLE 800
# elif defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE - 0) >= 700
#  define __XSI_VISIBLE 700
# elif defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE - 0) >= 600
#  define __XSI_VISIBLE 600
# elif defined(_XOPEN_SOURCE)
#  define __XSI_VISIBLE 500
# else
#  define __XSI_VISIBLE 0
# endif
# if __XSI_VISIBLE >= 800
#  define __XSI_POSIX 202405L
# elif __XSI_VISIBLE >= 700
#  define __XSI_POSIX 200809L
# elif __XSI_VISIBLE >= 600
#  define __XSI_POSIX 200112L
# elif __XSI_VISIBLE >= 500
#  define __XSI_POSIX 199506L
# else
#  define __XSI_POSIX 0
# endif
# if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE - 0) > __XSI_POSIX
#  define __POSIX_VISIBLE (_POSIX_C_SOURCE - 0)
# elif __XSI_POSIX
#  define __POSIX_VISIBLE __XSI_POSIX
# elif defined(_POSIX_C_SOURCE) || defined(_POSIX_SOURCE)
#  define __POSIX_VISIBLE 1L
# else
#  define __POSIX_VISIBLE 0
# endif
#endif

/* ISO C11/C17 interfaces (also part of POSIX.1-2024, which aligns with C17). */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || __POSIX_VISIBLE >= 202405L || \
    defined(__cplusplus)
#define __C11_VISIBLE 1
#else
#define __C11_VISIBLE 0
#endif

/* Interfaces of the X/Open System Interfaces (XSI) option that were marked
 * obsolescent and later removed, kept for programs written for those
 * editions of the standard.  Each is visible with no feature-test macro
 * (everything), and with _XOPEN_SOURCE of the editions that have it; never
 * with _POSIX_C_SOURCE alone, and never in POSIX.1-2024:
 *   __XSI_OB  SUSv2, SUSv3, SUSv4 (_XOPEN_SOURCE 500, 600, 700): gettimeofday,
 *             getitimer, setitimer, sighold, sigrelse, sigignore, sigpause,
 *             sigset, siginterrupt, setpgrp (removed in Issue 8, 2024)
 *   __XSI_OB6 SUSv2 and SUSv3 (500, 600): usleep (removed in Issue 7, 2008)
 * The editions' texts decide the windows (share/doc/legacy.md). */
#define __XSI_OB  (__EXT_VISIBLE || (__XSI_VISIBLE >= 500 && __XSI_VISIBLE < 800))
#define __XSI_OB6 (__EXT_VISIBLE || (__XSI_VISIBLE >= 500 && __XSI_VISIBLE < 700))

/* Portable spellings of a few qualifiers. */
#if defined(__cplusplus)
# define __RESTRICT
# define __NORETURN [[noreturn]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define __RESTRICT restrict
# if __STDC_VERSION__ >= 201112L
#  define __NORETURN _Noreturn
# else
#  define __NORETURN
# endif
#else
# define __RESTRICT
# define __NORETURN
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
# define __INLINE inline
#elif defined(__GNUC__)
# define __INLINE __inline__
#else
# define __INLINE
#endif

#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

#endif

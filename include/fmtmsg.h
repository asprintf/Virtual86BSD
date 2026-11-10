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

#ifndef _FMTMSG_H
#define _FMTMSG_H
#include <bits/features.h>

#define MM_HARD    0x001
#define MM_SOFT    0x002
#define MM_FIRM    0x004
#define MM_APPL    0x008
#define MM_UTIL    0x010
#define MM_OPSYS   0x020
#define MM_RECOVER 0x040
#define MM_NRECOV  0x080
#define MM_PRINT   0x100
#define MM_CONSOLE 0x200

#define MM_NOSEV   0
#define MM_HALT    1
#define MM_ERROR   2
#define MM_WARNING 3
#define MM_INFO    4

#define MM_NULLLBL ((char *)0)
#define MM_NULLSEV 0
#define MM_NULLMC  0L
#define MM_NULLTXT ((char *)0)
#define MM_NULLACT ((char *)0)
#define MM_NULLTAG ((char *)0)

#define MM_NOTOK (-1)
#define MM_OK    0
#define MM_NOMSG 1
#define MM_NOCON 4

__BEGIN_DECLS
int fmtmsg(long, const char *, int, const char *, const char *, const char *);
__END_DECLS

#endif

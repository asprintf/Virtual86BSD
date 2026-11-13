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

#ifndef _LANGINFO_H
#define _LANGINFO_H
#include <bits/features.h>
#include <nl_types.h>
#define __NEED_locale_t
#include <bits/alltypes.h>

/* Item numbers as in the GNU C library: category << 16 | the index of the
 * item in that category's locale file. */
#define CODESET      0x0000e
#define RADIXCHAR    0x10000
#define THOUSEP      0x10001
#define CRNCYSTR     0x4000f
#define YESEXPR      0x50000
#define NOEXPR       0x50001
#define ABDAY_1      0x20000
#define ABDAY_2      0x20001
#define ABDAY_3      0x20002
#define ABDAY_4      0x20003
#define ABDAY_5      0x20004
#define ABDAY_6      0x20005
#define ABDAY_7      0x20006
#define DAY_1        0x20007
#define DAY_2        0x20008
#define DAY_3        0x20009
#define DAY_4        0x2000a
#define DAY_5        0x2000b
#define DAY_6        0x2000c
#define DAY_7        0x2000d
#define ABMON_1      0x2000e
#define ABMON_2      0x2000f
#define ABMON_3      0x20010
#define ABMON_4      0x20011
#define ABMON_5      0x20012
#define ABMON_6      0x20013
#define ABMON_7      0x20014
#define ABMON_8      0x20015
#define ABMON_9      0x20016
#define ABMON_10     0x20017
#define ABMON_11     0x20018
#define ABMON_12     0x20019
#define MON_1        0x2001a
#define MON_2        0x2001b
#define MON_3        0x2001c
#define MON_4        0x2001d
#define MON_5        0x2001e
#define MON_6        0x2001f
#define MON_7        0x20020
#define MON_8        0x20021
#define MON_9        0x20022
#define MON_10       0x20023
#define MON_11       0x20024
#define MON_12       0x20025
#define AM_STR       0x20026
#define PM_STR       0x20027
#define D_T_FMT      0x20028
#define D_FMT        0x20029
#define T_FMT        0x2002a
#define T_FMT_AMPM   0x2002b
#define ERA          0x2002c
#define ERA_D_FMT    0x2002e
#define ALT_DIGITS   0x2002f
#define ERA_D_T_FMT  0x20030
#define ERA_T_FMT    0x20031
#define ALTMON_1     0x2006f
#define ALTMON_2     0x20070
#define ALTMON_3     0x20071
#define ALTMON_4     0x20072
#define ALTMON_5     0x20073
#define ALTMON_6     0x20074
#define ALTMON_7     0x20075
#define ALTMON_8     0x20076
#define ALTMON_9     0x20077
#define ALTMON_10    0x20078
#define ALTMON_11    0x20079
#define ALTMON_12    0x2007a
#define ABALTMON_1   0x20087
#define ABALTMON_2   0x20088
#define ABALTMON_3   0x20089
#define ABALTMON_4   0x2008a
#define ABALTMON_5   0x2008b
#define ABALTMON_6   0x2008c
#define ABALTMON_7   0x2008d
#define ABALTMON_8   0x2008e
#define ABALTMON_9   0x2008f
#define ABALTMON_10  0x20090
#define ABALTMON_11  0x20091
#define ABALTMON_12  0x20092

/* SUSv2 (LEGACY), removed in SUSv3; also in other systems */
#if __XSI_OB5
#define YESSTR       0x50002
#define NOSTR        0x50003
#endif

__BEGIN_DECLS
char *nl_langinfo(nl_item);
char *nl_langinfo_l(nl_item, locale_t);
__END_DECLS
#endif

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

#ifndef _WCTYPE_H
#define _WCTYPE_H
#include <bits/features.h>
#define __NEED_wint_t
#define __NEED_wctype_t
#define __NEED_wctrans_t
#if __POSIX_VISIBLE
#define __NEED_locale_t
#endif
#include <bits/alltypes.h>

#define WEOF 0xffffffffU

__BEGIN_DECLS
int       iswalnum(wint_t);
int       iswalpha(wint_t);
int       iswblank(wint_t);
int       iswcntrl(wint_t);
int       iswctype(wint_t, wctype_t);
int       iswdigit(wint_t);
int       iswgraph(wint_t);
int       iswlower(wint_t);
int       iswprint(wint_t);
int       iswpunct(wint_t);
int       iswspace(wint_t);
int       iswupper(wint_t);
int       iswxdigit(wint_t);
wint_t    towctrans(wint_t, wctrans_t);
wint_t    towlower(wint_t);
wint_t    towupper(wint_t);
wctrans_t wctrans(const char *);
wctype_t  wctype(const char *);
#if __POSIX_VISIBLE
int       iswalnum_l(wint_t, locale_t);
int       iswalpha_l(wint_t, locale_t);
int       iswblank_l(wint_t, locale_t);
int       iswcntrl_l(wint_t, locale_t);
int       iswctype_l(wint_t, wctype_t, locale_t);
int       iswdigit_l(wint_t, locale_t);
int       iswgraph_l(wint_t, locale_t);
int       iswlower_l(wint_t, locale_t);
int       iswprint_l(wint_t, locale_t);
int       iswpunct_l(wint_t, locale_t);
int       iswspace_l(wint_t, locale_t);
int       iswupper_l(wint_t, locale_t);
int       iswxdigit_l(wint_t, locale_t);
wint_t    towctrans_l(wint_t, wctrans_t, locale_t);
wint_t    towlower_l(wint_t, locale_t);
wint_t    towupper_l(wint_t, locale_t);
wctrans_t wctrans_l(const char *, locale_t);
wctype_t  wctype_l(const char *, locale_t);
#endif
__END_DECLS
#endif

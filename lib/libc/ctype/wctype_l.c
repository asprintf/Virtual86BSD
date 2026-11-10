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

#include <wctype.h>
#include <locale.h>
#include "locale_impl.h"

#define CT(l) ((l) == LC_GLOBAL_LOCALE ? __global_locale.cat[LC_CTYPE] : (l)->cat[LC_CTYPE])
#define F(name, cls) int name(wint_t c, locale_t l) { return __iswctype_l(c, cls + 1, CT(l)); }
F(iswalnum_l, W_ALNUM)
F(iswalpha_l, W_ALPHA)
F(iswblank_l, W_BLANK)
F(iswcntrl_l, W_CNTRL)
F(iswdigit_l, W_DIGIT)
F(iswgraph_l, W_GRAPH)
F(iswlower_l, W_LOWER)
F(iswprint_l, W_PRINT)
F(iswpunct_l, W_PUNCT)
F(iswspace_l, W_SPACE)
F(iswupper_l, W_UPPER)
F(iswxdigit_l, W_XDIGIT)

int iswctype_l(wint_t c, wctype_t t, locale_t l) { return __iswctype_l(c, t, CT(l)); }
wctype_t wctype_l(const char *name, locale_t l) { return __wctype_l(name, CT(l)); }
wint_t towupper_l(wint_t c, locale_t l) { return __towctrans_l(c, 1, CT(l)); }
wint_t towlower_l(wint_t c, locale_t l) { return __towctrans_l(c, 2, CT(l)); }
wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l) { return __towctrans_l(c, t, CT(l)); }
wctrans_t wctrans_l(const char *name, locale_t l) { return __wctrans_l(name, CT(l)); }

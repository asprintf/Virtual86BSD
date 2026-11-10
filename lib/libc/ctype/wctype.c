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
#include "locale_impl.h"

#define F(name, cls) int name(wint_t c) { return __iswctype_l(c, cls + 1, CURRENT_CTYPE); }
F(iswalnum, W_ALNUM)
F(iswalpha, W_ALPHA)
F(iswblank, W_BLANK)
F(iswcntrl, W_CNTRL)
F(iswdigit, W_DIGIT)
F(iswgraph, W_GRAPH)
F(iswlower, W_LOWER)
F(iswprint, W_PRINT)
F(iswpunct, W_PUNCT)
F(iswspace, W_SPACE)
F(iswupper, W_UPPER)
F(iswxdigit, W_XDIGIT)

int iswctype(wint_t c, wctype_t t) { return __iswctype_l(c, t, CURRENT_CTYPE); }
wctype_t wctype(const char *name) { return __wctype_l(name, CURRENT_CTYPE); }
wint_t towupper(wint_t c) { return __towctrans_l(c, 1, CURRENT_CTYPE); }
wint_t towlower(wint_t c) { return __towctrans_l(c, 2, CURRENT_CTYPE); }
wint_t towctrans(wint_t c, wctrans_t t) { return __towctrans_l(c, t, CURRENT_CTYPE); }
wctrans_t wctrans(const char *name) { return __wctrans_l(name, CURRENT_CTYPE); }

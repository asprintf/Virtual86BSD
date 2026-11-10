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

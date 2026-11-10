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

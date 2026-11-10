#include <locale.h>
#include <string.h>
#include <limits.h>
#include "locale_impl.h"

struct lconv *localeconv(void)
{
	static struct lconv lc;
	locale_t l = CURRENT_LOCALE;
	const struct __lcat *n = l->cat[LC_NUMERIC], *m = l->cat[LC_MONETARY];
#define S(c, i) ((char *)__lc_str(c, i))
/* -1 in a file (unspecified) is CHAR_MAX, as glibc gives it */
#define C(c, i) (*__lc_str(c, i) == -1 ? CHAR_MAX : *__lc_str(c, i))
	lc.decimal_point = S(n, 0);
	lc.thousands_sep = S(n, 1);
	lc.grouping = S(n, 2);
	lc.int_curr_symbol = S(m, 0);
	lc.currency_symbol = S(m, 1);
	lc.mon_decimal_point = S(m, 2);
	lc.mon_thousands_sep = S(m, 3);
	lc.mon_grouping = S(m, 4);
	lc.positive_sign = S(m, 5);
	lc.negative_sign = S(m, 6);
	lc.int_frac_digits = C(m, 7);
	lc.frac_digits = C(m, 8);
	lc.p_cs_precedes = C(m, 9);
	lc.p_sep_by_space = C(m, 10);
	lc.n_cs_precedes = C(m, 11);
	lc.n_sep_by_space = C(m, 12);
	lc.p_sign_posn = C(m, 13);
	lc.n_sign_posn = C(m, 14);
	lc.int_p_cs_precedes = C(m, 16);
	lc.int_p_sep_by_space = C(m, 17);
	lc.int_n_cs_precedes = C(m, 18);
	lc.int_n_sep_by_space = C(m, 19);
	lc.int_p_sign_posn = C(m, 20);
	lc.int_n_sign_posn = C(m, 21);
	return &lc;
}

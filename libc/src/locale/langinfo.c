#include <langinfo.h>
#include <locale.h>
#include "locale_impl.h"

char *nl_langinfo_l(nl_item item, locale_t l)
{
	int cat = item >> 16;
	unsigned idx = (unsigned)item & 0xffff;
	if (l == LC_GLOBAL_LOCALE) l = &__global_locale;
	if (cat < 0 || cat >= LC_ALL) return "";
	/* the codeset: of LC_CTYPE, whatever the category asked */
	if (item == CODESET) return (char *)__lc_str(l->cat[LC_CTYPE], 14);
	if (cat == LC_TIME && ((idx >= 111 && idx < 123) || (idx >= 135 && idx < 147)) &&
	    !*__lc_str(l->cat[cat], idx))
		/* files from before the alternative month names */
		return (char *)__lc_str(l->cat[cat], idx < 135 ? idx - 111 + 26 : idx - 135 + 14);
	return (char *)__lc_str(l->cat[cat], idx);
}

char *nl_langinfo(nl_item item)
{
	return nl_langinfo_l(item, CURRENT_LOCALE);
}

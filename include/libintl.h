#ifndef _LIBINTL_H
#define _LIBINTL_H
#include <bits/features.h>
#define __NEED_locale_t
#include <bits/alltypes.h>

__BEGIN_DECLS
char *bindtextdomain(const char *, const char *);
char *bind_textdomain_codeset(const char *, const char *);
char *dcgettext(const char *, const char *, int);
char *dcgettext_l(const char *, const char *, int, locale_t);
char *dcngettext(const char *, const char *, const char *, unsigned long, int);
char *dcngettext_l(const char *, const char *, const char *, unsigned long, int, locale_t);
char *dgettext(const char *, const char *);
char *dgettext_l(const char *, const char *, locale_t);
char *dngettext(const char *, const char *, const char *, unsigned long);
char *dngettext_l(const char *, const char *, const char *, unsigned long, locale_t);
char *gettext(const char *);
char *gettext_l(const char *, locale_t);
char *ngettext(const char *, const char *, unsigned long);
char *ngettext_l(const char *, const char *, unsigned long, locale_t);
char *textdomain(const char *);
__END_DECLS
#endif

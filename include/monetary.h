#ifndef _MONETARY_H
#define _MONETARY_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_locale_t
#include <bits/alltypes.h>

__BEGIN_DECLS
ssize_t strfmon(char *__RESTRICT, size_t, const char *__RESTRICT, ...);
ssize_t strfmon_l(char *__RESTRICT, size_t, locale_t, const char *__RESTRICT, ...);
__END_DECLS
#endif

/* Internal: multibyte conversion in a given locale's codeset. */
#ifndef _INTERNAL_MB_IMPL_H
#define _INTERNAL_MB_IMPL_H
#include <wchar.h>
struct __lcat;
size_t __mbrtowc_l(wchar_t *, const char *, size_t, mbstate_t *, const struct __lcat *);
size_t __wcrtomb_l(char *, wchar_t, const struct __lcat *);
size_t __wcrtomb_internal(char *, wchar_t);
#endif

#ifndef _UCHAR_H
#define _UCHAR_H
#include <bits/features.h>
#define __NEED_size_t
#define __NEED_mbstate_t
#include <bits/alltypes.h>

#if !defined(__cplusplus) || __cplusplus < 201103L
typedef unsigned short char16_t;
typedef unsigned char32_t;
#endif

__BEGIN_DECLS
size_t c16rtomb(char *__RESTRICT, char16_t, mbstate_t *__RESTRICT);
size_t c32rtomb(char *__RESTRICT, char32_t, mbstate_t *__RESTRICT);
size_t mbrtoc16(char16_t *__RESTRICT, const char *__RESTRICT, size_t, mbstate_t *__RESTRICT);
size_t mbrtoc32(char32_t *__RESTRICT, const char *__RESTRICT, size_t, mbstate_t *__RESTRICT);
__END_DECLS
#endif

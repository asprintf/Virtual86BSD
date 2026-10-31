/* <assert.h> -- deliberately without an include guard (ISO C 7.2). */
#include <bits/features.h>
#undef assert
#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
__BEGIN_DECLS
__NORETURN void __assert_fail(const char *, const char *, int, const char *);
__END_DECLS
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif
#if __C11_VISIBLE && !defined(__cplusplus) && !defined(static_assert)
#define static_assert _Static_assert
#endif

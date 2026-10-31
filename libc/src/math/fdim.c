#include "libm.h"

#define FUNCS(S, T, R) \
T fdim##S(T x, T y) \
{ \
	if (x != x || y != y) \
		return x + y; \
	if (x - x != 0 || y - y != 0) \
		return x > y ? x - y : 0; \
	return x > y ? R(x - y) : 0; \
} \
T fmax##S(T x, T y) \
{ \
	if (x != x) \
		return y; \
	if (y != y) \
		return x; \
	return x > y ? x : y; \
} \
T fmin##S(T x, T y) \
{ \
	if (x != x) \
		return y; \
	if (y != y) \
		return x; \
	return x < y ? x : y; \
}

/* x - y rounds in the type's own arithmetic; __math_* only add errno */
FUNCS(, double, __math_d)
FUNCS(f, float, __math_f)
FUNCS(l, long double, __math_l)

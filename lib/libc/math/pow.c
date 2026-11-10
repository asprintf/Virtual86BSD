#include "libm.h"

#define LOG2E_H 0x1.71547652B82FE176p0L
#define LOG2E_L 0x1.7D0FFDA0D23A7D11D6AEF551BAD2B4B1p-64L

/* log2(x) = hi + lo to about 2^-80 relative, x > 0 finite:
 * x = 2^k m, m in [sqrt(1/2), sqrt(2)), ln m = 2 atanh s = 2s + 2s^3 P(s^2),
 * s = (m - 1)/(m + 1) computed as s_h + s_l */
void __log2l_hl(long double x, long double *hi, long double *lo)
{
	union __ldbits u = {x};
	int k = 0, i;
	long double m, f, uh, ul, sh, sl, p, pl, z, P, a, b, H, t;
	static const long double c[] = {
		1.0L/3, 1.0L/5, 1.0L/7, 1.0L/9, 1.0L/11, 1.0L/13, 1.0L/15, 1.0L/17,
		1.0L/19, 1.0L/21, 1.0L/23, 1.0L/25, 1.0L/27, 1.0L/29, 1.0L/31, 1.0L/33,
	};
	if ((u.i.se & 0x7fff) == 0) {          /* subnormal */
		u.f *= 0x1p64L;
		k = -64;
	}
	k += (u.i.se & 0x7fff) - 16383;
	u.i.se = 16383;                         /* m in [1, 2) */
	if (u.i.m > 0xB504F333F9DE6484ULL) {    /* m > sqrt 2 */
		u.i.se = 16382;
		k++;
	}
	m = u.f;
	f = m - 1;
	__two_sum(m, 1, &uh, &ul);
	sh = f / uh;
	__two_prod(sh, uh, &p, &pl);
	sl = (((f - p) - pl) - sh * ul) / uh;
	z = sh * sh;
	P = c[15];
	for (i = 14; i >= 0; i--)
		P = P * z + c[i];
	a = 2 * sh;
	b = 2 * sl + a * z * P;
	/* (a + b) log2 e */
	__two_prod(a, LOG2E_H, &p, &pl);
	pl += a * LOG2E_L + b * LOG2E_H;
	__two_sum((long double)k, p, &H, &t);
	__two_sum(H, t + pl, hi, lo);
}

/* s * 2^(hi + lo), |lo| <= 2^-60 |hi|, for -16446 <= hi < 16384 */
long double __exp2l_hl(long double hi, long double lo, long double s)
{
	long double n = __x87_rint(hi);
	long double f = (hi - n) + lo;
	return __x87_scale(s * (1 + __x87_2xm1(f)), n);
}

/* y is an integer: 1 if odd, 2 if even; 0 if not an integer */
static int intkind(long double y)
{
	long double t;
	if (__ld_nonfinite(y))
		return 0;
	if (__ld_abs(y) >= 0x1p64L)
		return 2;
	t = __x87_rint(y);
	if (t != y)
		return 0;
	return __x87_rint(y / 2) * 2 == y ? 2 : 1;
}

long double powl(long double x, long double y)
{
	long double hi, lo, h, l, s = 1, ax;
	int k;

	if (y == 0 || x == 1)
		return 1;
	if (x != x || y != y)
		return x + y;
	k = intkind(y);
	if (x == 0) {
		int neg = k == 1 && 1 / x < 0;
		if (y < 0)
			/* (0, -inf): +inf, and the pole error POSIX allows ("may"),
			   as for a negative finite y */
			return __ld_nonfinite(y) ? __math_divzero(0) : __math_divzero(neg);
		return neg ? -0.0L : 0.0L;
	}
	ax = __ld_abs(x);
	if (__ld_nonfinite(y)) {                       /* y = +-inf */
		if (ax == 1)
			return 1;
		return (ax > 1) == (y > 0) ? y * y : 0;
	}
	if (__ld_nonfinite(x)) {                       /* x = +-inf */
		int neg = x < 0 && k == 1;
		if (y < 0)
			return neg ? -0.0L : 0.0L;
		return neg ? x : ax;
	}
	if (x < 0) {
		if (k == 0)
			return __math_invalid(x);
		if (k == 1)
			s = -1;
	}
	/* |x|^y = 2^(y log2 |x|) */
	__log2l_hl(ax, &h, &l);
	__two_prod(y, h, &hi, &lo);
	lo += y * l;
	__two_sum(hi, lo, &hi, &lo);
	if (hi >= 16384)
		return __math_oflow(s < 0);
	if (hi < -16447)
		return __math_uflow(s < 0);
	return __math_l(__exp2l_hl(hi, lo, s));
}

__MATH_WRAP2(pow)

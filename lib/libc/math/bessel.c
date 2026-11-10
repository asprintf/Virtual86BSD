/* The Bessel functions of the first and second kinds (XSI), in long
 * double:
 *  x < 25:  J_k by Miller's backward recurrence, normalized by
 *           J_0 + 2 J_2 + 2 J_4 + ... = 1; Y_0 and Y_1 by the Neumann
 *           series in the same J_k
 *  x >= 25: J_0, J_1, Y_0, Y_1 by Hankel's asymptotic expansions;
 *           J_n by forward recurrence for n < x, else by Miller's
 *  Y_n:     forward recurrence from Y_0, Y_1 (always stable)
 */
#include "libm.h"

#define TWO_OVER_PI 0.636619772367581343075535053490057448L

/* Miller: J_n(x), and (if s != 0) J_0, J_1 and the Neumann sums
 * s[0] = J_0, s[1] = J_1, s[2] = sum_k>=1 (-1)^k J_2k / k,
 * s[3] = sum_k>=1 (-1)^k (J_2k-1 - J_2k+1) / k;  x > 0 */
static long double miller(int n, long double x, long double *s)
{
	int m = n > x ? n : (int)x, N, k;
	long double f1 = 0, f = 0x1p-1000L, f0, norm = 0, jn = 0, j1 = 0, s0 = 0, s1 = 0;
	N = m + 40 + (int)(x / 2);
	N += N & 1;
	for (k = N; k >= 1; k--) {
		/* f = f_k, f1 = f_k+1 */
		if (k == n)
			jn = f;
		if (k == 1)
			j1 = f;
		if (k & 1) {
			int h = (k + 1) / 2;
			s1 += (h & 1 ? -f : f) / h;
			if (k >= 3) {
				h = (k - 1) / 2;
				s1 -= (h & 1 ? -f : f) / h;
			}
		} else {
			norm += 2 * f;
			s0 += ((k / 2) & 1 ? -f : f) / (k / 2);
		}
		f0 = 2 * k / x * f - f1;
		f1 = f;
		f = f0;
		if (__ld_abs(f) > 0x1p1000L) {
			f *= 0x1p-1000L; f1 *= 0x1p-1000L; norm *= 0x1p-1000L;
			jn *= 0x1p-1000L; j1 *= 0x1p-1000L; s0 *= 0x1p-1000L; s1 *= 0x1p-1000L;
		}
	}
	/* f = f_0 */
	norm += f;
	if (n == 0)
		jn = f;
	if (s) {
		s[0] = f / norm;
		s[1] = j1 / norm;
		s[2] = s0 / norm;
		s[3] = s1 / norm;
	}
	return jn / norm;
}

/* Hankel: P and Q of order n (0 or 1) at x >= 25 */
static void hankel(int n, long double x, long double *p, long double *q)
{
	long double mu = 4 * n * n, t = 1, P = 1, Q = 0, prev = 2;
	int k;
	for (k = 1; k < 120; k++) {
		t *= (mu - (2 * k - 1) * (2 * k - 1)) / (k * 8 * x);
		if (__ld_abs(t) >= prev || t == 0)
			break;
		prev = __ld_abs(t);
		switch (k & 3) {
		case 1: Q += t; break;
		case 2: P -= t; break;
		case 3: Q -= t; break;
		default: P += t; break;
		}
	}
	*p = P;
	*q = Q;
}

/* J_n(x) and Y_n(x) for n = 0, 1 and x >= 25 */
static void large01(int n, long double x, long double *j, long double *y)
{
	long double p, q, s = sinl(x), c = cosl(x), cc, ss, a;
	hankel(n, x, &p, &q);
	/* chi = x - pi/4 - n pi/2: cos chi, sin chi */
	if (n == 0) {
		cc = (c + s) * M_SQRT1_2l;
		ss = (s - c) * M_SQRT1_2l;
	} else {
		cc = (s - c) * M_SQRT1_2l;
		ss = -(s + c) * M_SQRT1_2l;
	}
	a = __x87_sqrt(TWO_OVER_PI / x);
	if (j)
		*j = a * (p * cc - q * ss);
	if (y)
		*y = a * (p * ss + q * cc);
}

static long double jnl(int n, long double x)
{
	long double sign = 1, a, b, t;
	int k;
	if (n < 0) {
		n = -n;
		if (n & 1)
			sign = -1;
	}
	if (x < 0) {
		x = -x;
		if (n & 1)
			sign = -sign;
	}
	if (x != x)
		return x;
	if (__ld_nonfinite(x))
		return 0;
	if (x == 0)
		return n ? 0 : 1;
	if (x < 0x1p-40L) {         /* (x/2)^n / n! */
		t = 1;
		for (k = 1; k <= n && t != 0; k++)
			t *= x / (2 * k);
		return sign * t;
	}
	if (n > x + 20 && n * __x87_yl2x(2.718281828459045L * x / (2 * n), __LN2) < -11450)
		return sign * 0;            /* below the smallest long double */
	if (x < 25 || n >= x)
		return sign * miller(n, x, 0);
	large01(0, x, &a, 0);
	if (n == 0)
		return sign * a;
	large01(1, x, &b, 0);
	for (k = 1; k < n; k++) {
		t = 2 * k / x * b - a;
		a = b;
		b = t;
	}
	return sign * b;
}

/* Y_0 and Y_1, x > 0 finite */
static void y01(long double x, long double *y0, long double *y1)
{
	long double s[4], l;
	if (x >= 25) {
		large01(0, x, 0, y0);
		large01(1, x, 0, y1);
		return;
	}
	if (x < 0x1p-40L) {
		l = __x87_yl2x(x / 2, __LN2) + __EGAMMA;
		*y0 = TWO_OVER_PI * l;
		*y1 = -TWO_OVER_PI / x;
		return;
	}
	miller(0, x, s);
	l = __x87_yl2x(x / 2, __LN2) + __EGAMMA;
	*y0 = TWO_OVER_PI * (l * s[0] - 2 * s[2]);
	*y1 = TWO_OVER_PI * (l * s[1] - s[0] / x + s[3]);
}

static long double ynl(int n, long double x)
{
	long double sign = 1, a, b, t;
	int k;
	if (n < 0) {
		n = -n;
		if (n & 1)
			sign = -1;
	}
	if (x != x)
		return x;
	if (x < 0)
		return __math_invalid(x);
	if (x == 0)
		return sign * __math_divzero(1);
	if (__ld_nonfinite(x))
		return 0;
	y01(x, &a, &b);
	if (n == 0)
		return a;
	for (k = 1; k < n && !__ld_nonfinite(b); k++) {
		t = 2 * k / x * b - a;
		a = b;
		b = t;
	}
	return sign * b;
}

double j0(double x) { return __math_d(jnl(0, x)); }
double j1(double x) { return __math_d(jnl(1, x)); }
double jn(int n, double x) { return __math_d(jnl(n, x)); }
double y0(double x) { return __math_d(ynl(0, x)); }
double y1(double x) { return __math_d(ynl(1, x)); }
double yn(int n, double x) { return __math_d(ynl(n, x)); }

/* s_math.c - <math.h> and <fenv.h> checks that need no other library:
 * the Bessel functions against exact values (bessel_ref.h, from bc),
 * the floating-point environment, rounding modes, the classification
 * macros, fma in all rounding modes. */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const struct { int n; double x; long double j, y; } bes[] = {
#include "bessel_ref.h"
};

/* error of v against the exact r in ulps of double */
static double ulps(double v, long double r)
{
	int e;
	if (r == 0)
		return v == 0 ? 0 : 1e30;
	frexpl(r, &e);
	if (e < -1021)
		e = -1021;
	return (double)(fabsl(v - r) / ldexpl(1, e - 53));
}

static void bessel(void)
{
	size_t i;
	double worst = 0;
	for (i = 0; i < sizeof bes / sizeof bes[0]; i++) {
		double x = bes[i].x, j = jn(bes[i].n, x), y = yn(bes[i].n, x);
		double ej = ulps(j, bes[i].j), ey = ulps(y, bes[i].y);
		/* near a zero only an absolute error makes sense: up to
		 * 2^-62 times the size of the functions there */
		long double env = x > 1 ? 0.8L / sqrtl(x) : 1;
		if (fabsl(j - bes[i].j) <= env * 0x1p-62L) ej = 0;
		if (fabsl(y - bes[i].y) <= env * 0x1p-62L) ey = 0;
		if (ej > 2 || ey > 2) {
			printf("FAIL: jn/yn(%d, %a) = %a %a, exact %.20Lg %.20Lg (%g, %g ulp)\n",
			       bes[i].n, x, j, y, bes[i].j, bes[i].y, ej, ey);
			fails++;
		}
		if (ej > worst) worst = ej;
		if (ey > worst) worst = ey;
		if (bes[i].n == 0) {
			CHECK(j0(x) == j && y0(x) == y);
		}
		if (bes[i].n == 1) {
			CHECK(j1(x) == j && y1(x) == y);
		}
		CHECK(jn(-bes[i].n, -x) == j);
		CHECK(yn(-bes[i].n, x) == (bes[i].n & 1 ? -y : y));
	}
	printf("Bessel functions: largest error %g ulp\n", worst);
}

static void environment(void)
{
	volatile double one = 1, zero = 0, three = 3, big = 1e308, r;
	volatile long double lone = 1, lzero = 0, lr;
	fenv_t env, env2;
	fexcept_t fl;

	CHECK(feclearexcept(FE_ALL_EXCEPT) == 0);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == 0);
	r = one / zero;                       /* SSE */
	CHECK(fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
	feclearexcept(FE_DIVBYZERO);
	lr = lone / lzero;                    /* x87 */
	CHECK(fetestexcept(FE_DIVBYZERO));
	feclearexcept(FE_ALL_EXCEPT);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == 0);
	r = big * big;
	CHECK(fetestexcept(FE_OVERFLOW | FE_INEXACT) == (FE_OVERFLOW | FE_INEXACT));
	feclearexcept(FE_ALL_EXCEPT);
	r = zero / zero;
	CHECK(fetestexcept(FE_INVALID));
	feclearexcept(FE_ALL_EXCEPT);

	CHECK(feraiseexcept(FE_UNDERFLOW | FE_INEXACT) == 0);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == (FE_UNDERFLOW | FE_INEXACT));
	CHECK(fegetexceptflag(&fl, FE_ALL_EXCEPT) == 0);
	feclearexcept(FE_ALL_EXCEPT);
	CHECK(fesetexceptflag(&fl, FE_INEXACT) == 0);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == FE_INEXACT);
	feclearexcept(FE_ALL_EXCEPT);

	/* rounding modes: SSE and x87 arithmetic and the functions (called
	 * through volatile pointers: a compiler that assumes the default
	 * rounding mode may evaluate them once only) */
	double (*volatile rint_)(double) = rint, (*volatile nearbyint_)(double) = nearbyint;
	float (*volatile rintf_)(float) = rintf;
	long double (*volatile rintl_)(long double) = rintl;
	long (*volatile lrint_)(double) = lrint;
	long long (*volatile llrint_)(double) = llrint;
	static const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
	static const double rints[][4] = {      /* rint of 2.5, -2.5, 2.25, -2.75 */
		{2, -2, 2, -3}, {2, -3, 2, -3}, {3, -2, 3, -2}, {2, -2, 2, -2},
	};
	int m;
	for (m = 0; m < 4; m++) {
		CHECK(fesetround(modes[m]) == 0);
		CHECK(fegetround() == modes[m]);
		volatile double a = 2.5, b = -2.5, c = 2.25, d = -2.75;
		CHECK(rint_(a) == rints[m][0] && rint_(b) == rints[m][1]);
		CHECK(rintf_((float)c) == rints[m][2] && rintl_(d) == rints[m][3]);
		CHECK(lrint_(a) == (long)rints[m][0] && llrint_(d) == (long long)rints[m][3]);
		CHECK(nearbyint_(b) == rints[m][1]);
		r = one / three;
		lr = lone / three;
		if (modes[m] == FE_UPWARD) {
			CHECK(r * 3 > 1 || r > 0.333333333333333);
			CHECK((double)lr >= r - 1e-16);
		}
		/* 1/3 differs between downward and upward in both units */
		{
			volatile double up, down;
			volatile long double lup, ldown;
			fesetround(FE_UPWARD); up = one / three; lup = lone / three;
			fesetround(FE_DOWNWARD); down = one / three; ldown = lone / three;
			CHECK(up > down && lup > ldown);
			fesetround(modes[m]);
		}
	}
	CHECK(fesetround(12345) != 0);
	fesetround(FE_TONEAREST);

	/* nearbyint raises no inexact; rint does */
	feclearexcept(FE_ALL_EXCEPT);
	r = nearbyint(2.5 * one);
	CHECK(!fetestexcept(FE_INEXACT));
	r = rint(2.5 * one);
	CHECK(fetestexcept(FE_INEXACT));

	/* environments */
	feclearexcept(FE_ALL_EXCEPT);
	fesetround(FE_UPWARD);
	feraiseexcept(FE_OVERFLOW);
	CHECK(fegetenv(&env) == 0);
	CHECK(fesetenv(FE_DFL_ENV) == 0);
	CHECK(fegetround() == FE_TONEAREST && fetestexcept(FE_ALL_EXCEPT) == 0);
	CHECK(fesetenv(&env) == 0);
	CHECK(fegetround() == FE_UPWARD && fetestexcept(FE_ALL_EXCEPT) == FE_OVERFLOW);
	CHECK(feholdexcept(&env2) == 0);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == 0 && fegetround() == FE_UPWARD);
	r = one / zero;
	CHECK(feupdateenv(&env2) == 0);
	CHECK(fetestexcept(FE_ALL_EXCEPT) == (FE_OVERFLOW | FE_DIVBYZERO));
	fesetenv(FE_DFL_ENV);
	(void)r; (void)lr;
}

/* fma: exact intermediate, one rounding in each mode */
static double (*const fma_)(double, double, double) = fma;
static void fmas(void)
{
	double (*volatile fma)(double, double, double) = fma_;
	volatile double a = 1 + 0x1p-52, b = 1 - 0x1p-53;
	/* a*b = 1 + 2^-53 - 2^-105: nearest 1, up 1 + 2^-52 */
	CHECK(fma(a, b, 0) == 1);
	fesetround(FE_UPWARD);
	CHECK(fma(a, b, 0) == 1 + 0x1p-52);
	CHECK(fma(a, b, -1) == 0x1p-53 - 0x1p-105);
	fesetround(FE_DOWNWARD);
	CHECK(fma(a, b, 0) == 1);
	CHECK(fma(a, -a, 1) < 0);
	CHECK(fma(1.0, 1.0, -1.0) == 0 && signbit(fma(1.0, 1.0, -1.0)));
	fesetround(FE_TOWARDZERO);
	CHECK(fma(DBL_MAX, 2, 0) == DBL_MAX);
	fesetround(FE_TONEAREST);
	CHECK(fma(DBL_MAX, 2, 0) == INFINITY);
	CHECK(fma(0x1p-1000, 0x1p-74, 0) == 0x1p-1074);
	CHECK(fma(0x1p-1000, 0x1.8p-75, 0) == 0x1p-1074);   /* 0.75 of the least: up */
	CHECK(fmaf(0x1p-100f, 0x1p-49f, 0) == 0x1p-149f);
	CHECK(fmal(1 + 0x1p-32L, 1 + 0x1p-32L, -1) == 0x1p-31L + 0x1p-64L);
	CHECK(fmal(-0.0L, 1, 0.0L) == 0 && !signbit(fmal(-0.0L, 1, 0.0L)));
}

static void classify(void)
{
	volatile float f = 1e-40f;            /* subnormal */
	volatile double d = 1e-310, nan = NAN, inf = INFINITY;
	volatile long double l = 1e-4940L, ln = NAN;
	CHECK(fpclassify(f) == FP_SUBNORMAL && fpclassify(d) == FP_SUBNORMAL && fpclassify(l) == FP_SUBNORMAL);
	CHECK(fpclassify(1.0f) == FP_NORMAL && fpclassify(0.0) == FP_ZERO && fpclassify(HUGE_VALL) == FP_INFINITE);
	CHECK(fpclassify(ln) == FP_NAN && isnan(ln) && isnan(nan) && !isnan(inf));
	CHECK(isinf(inf) && isinf(-inf) && !isinf(d) && isfinite(d) && !isfinite(nan));
	CHECK(!isnormal(f) && isnormal(1.0L) && isnormal(1.0f));
	CHECK(signbit(-0.0) && !signbit(0.0f) && signbit(-1.0L) && signbit(-nan) != signbit(nan));
	feclearexcept(FE_ALL_EXCEPT);
	CHECK(!isgreater(nan, 1) && !isless(1.0f, nan) && !islessgreater(nan, nan) && isunordered(1, nan));
	CHECK(isgreaterequal(2, 1.5) && islessequal(1.0L, 1) && islessgreater(1.0f, 2.0));
	CHECK(fetestexcept(FE_INVALID) == 0);
	CHECK(sizeof(float_t) == sizeof(float) && sizeof(double_t) == sizeof(double));
	CHECK(math_errhandling == (MATH_ERRNO | MATH_ERREXCEPT));
	CHECK(M_PI == 3.14159265358979323846 && M_PIl == 3.141592653589793238462643383279502884L);
	CHECK(M_SQRT3 * M_SQRT1_3 >= 1 - 0x1p-52 && M_PHI * M_PHI - M_PHI - 1 < 1e-15);
	CHECK(HUGE_VAL == INFINITY && HUGE_VALF == INFINITY && HUGE_VALL == INFINITY);
	{
		static const float sf = INFINITY, sn = NAN;   /* constant expressions */
		CHECK(isinf(sf) && isnan(sn) && !signbit(sn));
	}
}

/* exact results that the tolerance of u_math.c would let pass */
static void exact(void)
{
	static const double xs[] = {1, 8, 27, 64, 1e-300 * 1e-300 + 0x1p-1074};
	CHECK(cbrt(27.0) == 3 && cbrt(-8.0) == -2 && cbrtl(1000.0L) == 10 && cbrtf(64.0f) == 4);
	CHECK(pow(2, 10) == 1024 && pow(10, 22) == 1e22 && powl(3, 40) == 12157665459056928801.0L);
	CHECK(pow(-2, 3) == -8 && pow(-8, 1.0 / 3) != pow(-8, 1.0 / 3));
	CHECK(exp2(10) == 1024 && exp2l(-16445) == 0x1p-16445L && log2(1024) == 10 && log10(1e22) == 22);
	CHECK(sqrt(2) * sqrt(2) != 2 && sqrtl(4) == 2 && hypot(3, 4) == 5 && hypotl(5, 12) == 13);
	CHECK(exp(0) == 1 && log(1) == 0 && !signbit(log(1)) && sin(-0.0) == 0 && signbit(sin(-0.0)));
	CHECK(atan2(0.0, -0.0) == M_PI && atan2(-0.0, -1) == -M_PI && atan2(INFINITY, -INFINITY) == 3 * M_PI_4);
	CHECK(tgamma(5) == 24 && fabs(tgamma(0.5) - sqrt(M_PI)) <= 0x1p-52 && tgammal(171) > 7e306L);
	CHECK(lgamma(1) == 0 && lgamma(2) == 0 && fabsl(lgammal(3) - M_LN2l) <= 0x1p-64L);
	lgamma(-0.5);
	CHECK(signgam == -1);
	lgamma(-1.5);
	CHECK(signgam == 1);
	CHECK(erf(0) == 0 && erfc(0) == 1 && erf(INFINITY) == 1 && erfc(-INFINITY) == 2);
	CHECK(remquo(10, 3, &(int){0}) == 1 && remainder(10, 4) == 2 && remainder(14, 4) == -2);
	CHECK(fmod(-7, 3) == -1 && fmodl(0x1p16000L, 3) == fmodl(0x1p16000L, 3) && fmod(5e300, 7) < 7);
	(void)xs;
}

int main(void)
{
	bessel();
	environment();
	fmas();
	classify();
	exact();
	errno = 0;
	CHECK(isnan(sqrt(-1)) && errno == EDOM);
	printf("%d failures\n", fails);
	return fails != 0;
}

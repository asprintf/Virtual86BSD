/* u_complex.c - <complex.h> against the system's library, as u_math.c:
 * "u_complex > ref" with the system's library, "u_complex -c < ref"
 * with this one.  The error of a result is that of its worse part in
 * units in the last place of the larger part (the usual measure for
 * complex functions); special values must agree exactly, but for the
 * signs of NaNs, of zeros and of infinities beside a NaN, which C
 * mostly leaves unspecified ("+-0", "+-inf + i NaN"). */
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check, fails;
static double worst[64];
static const char *wname[64];
static int nw;

static uint64_t seed = 0x2545F4914F6CDD1DULL;
static uint64_t rnd(void)
{
	seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
	return seed;
}
static double unif(double a, double b) { return a + (b - a) * (double)(rnd() >> 11) * 0x1p-53; }

/* the unit in the last place of x, at least that of the subnormals */
static long double ulp(long double x, int prec)
{
	int e, emin = prec == 24 ? -125 : prec == 53 ? -1021 : -16381;
	frexpl(x, &e);
	if (x == 0 || e < emin)
		e = emin;
	return ldexpl(1, e - prec);
}

static int isinfl_(long double x) { return x - x != 0 && x == x; }

/* one result, both parts as long double */
static void out(const char *name, int prec, long double a, long double b, long double x, long double y,
                double tol, int special)
{
	char line[256];
	int i;
	if (!check) {
		printf("%La %La\n", a, b);
		return;
	}
	long double ra, rb;
	if (!fgets(line, sizeof line, stdin) || sscanf(line, "%La %La", &ra, &rb) != 2) {
		printf("FAIL: reference output ends at %s\n", name);
		exit(1);
	}
	double e;
	if (special) {
		/* NaN equals NaN; zeros equal zeros (their sign is mostly
		 * unspecified for these); otherwise exact */
		int ok = ((a != a && ra != ra) || (a == 0 && ra == 0) || a == ra ||
		          (rb != rb && isinfl_(a) && isinfl_(ra))) &&
		         ((b != b && rb != rb) || (b == 0 && rb == 0) || b == rb ||
		          (ra != ra && isinfl_(b) && isinfl_(rb)));
		e = ok ? 0 : 1e9;
	} else {
		long double fa = isinfl_(ra) ? 0 : fabsl(ra), fb = isinfl_(rb) ? 0 : fabsl(rb);
		long double m = fa > fb ? fa : fb, u = ulp(m, prec);
		if ((a != a) != (ra != ra) || (b != b) != (rb != rb))
			e = 1e9;
		else {
			long double da = a == ra ? 0 : fabsl(a - ra), db = b == rb ? 0 : fabsl(b - rb);
			e = (double)((da > db ? da : db) / u);
			if (e != e)
				e = 0;   /* inf - inf: same infinities */
		}
	}
	for (i = 0; i < nw && strcmp(wname[i], name); i++)
		;
	if (i == nw && nw < 64)
		wname[nw++] = name;
	if (i < 64 && e > worst[i])
		worst[i] = e;
	if (e > tol) {
		printf("FAIL: %s(%La + %La i) = %La + %La i, expected %La + %La i (%g ulp)\n",
		       name, x, y, a, b, ra, rb, e);
		fails++;
	}
}

typedef double _Complex (*cd)(double _Complex);
typedef float _Complex (*cf)(float _Complex);
typedef long double _Complex (*cl)(long double _Complex);
static const struct { const char *name; cd d; cf f; cl l; double tol; } F[] = {
	{"cexp", cexp, cexpf, cexpl, 2}, {"clog", clog, clogf, clogl, 2},
	{"csqrt", csqrt, csqrtf, csqrtl, 2},
	{"csin", csin, csinf, csinl, 2}, {"ccos", ccos, ccosf, ccosl, 2}, {"ctan", ctan, ctanf, ctanl, 4},
	{"csinh", csinh, csinhf, csinhl, 2}, {"ccosh", ccosh, ccoshf, ccoshl, 2}, {"ctanh", ctanh, ctanhf, ctanhl, 4},
	{"casin", casin, casinf, casinl, 4}, {"cacos", cacos, cacosf, cacosl, 4}, {"catan", catan, catanf, catanl, 4},
	{"casinh", casinh, casinhf, casinhl, 4}, {"cacosh", cacosh, cacoshf, cacoshl, 4},
	{"catanh", catanh, catanhf, catanhl, 4}, {"cproj", cproj, cprojf, cprojl, 0}, {"conj", conj, conjf, conjl, 0},
};

static void one(int i, double x, double y, int special)
{
	double _Complex d = F[i].d(CMPLX(x, y));
	float _Complex f = F[i].f(CMPLXF(x, y));
	long double _Complex l = F[i].l(CMPLXL(x, y));
	static char nf[64][16], nl[64][16];
	sprintf(nf[i], "%sf", F[i].name);
	sprintf(nl[i], "%sl", F[i].name);
	out(F[i].name, 53, creal(d), cimag(d), x, y, F[i].tol, special);
	out(nf[i], 24, crealf(f), cimagf(f), (float)x, (float)y, F[i].tol, special);
	out(nl[i], 64, creall(l), cimagl(l), x, y, F[i].tol * 2, special);
}

int main(int argc, char **argv)
{
	static const double sv[] = {0.0, -0.0, 1, -1, 0.5, -2, 1e300, INFINITY, -INFINITY, NAN};
	size_t i, j, k;
	check = argc > 1 && !strcmp(argv[1], "-c");
	for (i = 0; i < sizeof F / sizeof F[0]; i++) {
		for (j = 0; j < sizeof sv / sizeof sv[0]; j++)
			for (k = 0; k < sizeof sv / sizeof sv[0]; k++)
				if (sv[j] - sv[j] != 0 || sv[k] - sv[k] != 0 || (sv[j] == 0 && sv[k] == 0))
					one((int)i, sv[j], sv[k], 1);
		for (k = 0; k < 400; k++) {
			double x, y;
			switch (k % 4) {
			case 0: x = unif(-3, 3); y = unif(-3, 3); break;
			case 1: x = unif(-30, 30); y = unif(-0.1, 0.1); break;
			case 2: x = unif(-1.5, 1.5); y = unif(-1e-3, 1e-3); break;
			default: x = unif(-1, 1) * 1e5; y = unif(-1, 1) * 1e5; break;
			}
			if (F[i].d == cexp || F[i].d == csinh || F[i].d == ccosh || F[i].d == csin || F[i].d == ccos) {
				/* moderate arguments: e^x times the parts */
				if (k % 4 == 3) { x = unif(-300, 300); y = unif(-1e3, 1e3); }
				if (F[i].d == csin || F[i].d == ccos) { double t = x; x = y; y = t; }
			}
			one((int)i, x, y, 0);
		}
	}
	/* cabs, carg, cpow */
	for (k = 0; k < 400; k++) {
		double x = unif(-10, 10), y = unif(-10, 10), u = unif(-3, 3), v = unif(-3, 3);
		out("cabs", 53, cabs(CMPLX(x, y)), 0, x, y, 1, 0);
		out("cargl", 64, cargl(CMPLXL(x, y)), 0, x, y, 2, 0);
		double _Complex p = cpow(CMPLX(x, y), CMPLX(u, v));
		out("cpow", 53, creal(p), cimag(p), x, y, 64, 0);
	}
	if (check) {
		for (i = 0; i < (size_t)nw; i++)
			printf("%-8s max error %g ulp\n", wname[i], worst[i]);
		printf("%d failures\n", fails);
	}
	return check && fails;
}

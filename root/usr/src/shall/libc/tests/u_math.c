/* u_math.c - <math.h> against the system's library, within a tolerance
 * in units in the last place (ulps).
 *
 * Built twice, as the other u_*.c tests (tests/run.sh): against the
 * system's library it writes the results ("u_math > ref"); against
 * this one it reads those and compares ("u_math -c < ref"), printing
 * FAIL lines and, at the end, the largest error of each function.
 * The arguments are special values (whose results, errno included,
 * must agree exactly but for a few documented cases) and pseudo-random
 * values over each function's domain.
 */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check;          /* -c: compare with the reference on stdin */
static int fails;

/* --- a deterministic generator --- */
static uint64_t seed = 0x9E3779B97F4A7C15ULL;
static uint64_t rnd(void)
{
	seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
	return seed;
}
static double unif(double a, double b) { return a + (b - a) * (double)(rnd() >> 11) * 0x1p-53; }
/* sign * about 10^u, u uniform in [lo, hi] (without the library: the
 * arguments must be the same in both builds) */
static double logu(double lo, double hi, int sign)
{
	int e = (int)(unif(lo, hi) * 3.321928);
	double v = unif(1, 2);
	for (; e > 0; e--) v *= 2;
	for (; e < 0; e++) v /= 2;
	return sign && (rnd() & 1) ? -v : v;
}

/* --- results: kind 'd', 'f', 'l', 'i' (integer) --- */
struct res { char kind; uint64_t hi, lo; int err; };

static struct res rd(double x) { struct res r = {'d', 0, 0, 0}; memcpy(&r.lo, &x, 8); return r; }
static struct res rf(float x) { struct res r = {'f', 0, 0, 0}; uint32_t u; memcpy(&u, &x, 4); r.lo = u; return r; }
static struct res rl(long double x)
{
	struct res r = {'l', 0, 0, 0};
	unsigned char b[16] = {0};
	uint16_t se;
	memcpy(b, &x, 10);
	memcpy(&r.lo, b, 8);
	memcpy(&se, b + 8, 2);
	r.hi = se;
	return r;
}
static struct res ri(long long x) { struct res r = {'i', 0, (uint64_t)x, 0}; return r; }

/* ordered integers of the bit patterns: ulp distances are differences */
static long double ord(const struct res *r, int *nan)
{
	*nan = 0;
	if (r->kind == 'd') {
		double x; memcpy(&x, &r->lo, 8);
		if (x != x) { *nan = 1; return 0; }
		int64_t i = (int64_t)(r->lo & ~(1ULL << 63));
		return r->lo >> 63 ? -(long double)i : (long double)i;
	}
	if (r->kind == 'f') {
		uint32_t u = (uint32_t)r->lo; float x; memcpy(&x, &u, 4);
		if (x != x) { *nan = 1; return 0; }
		long double i = u & 0x7fffffff;
		return u >> 31 ? -i : i;
	}
	if (r->kind == 'l') {
		int e = r->hi & 0x7fff;
		if (e == 0x7fff && r->lo << 1) { *nan = 1; return 0; }
		/* the significand without the integer bit, continuous across binades */
		long double i = (long double)e * 0x1p63L + (long double)(r->lo & ~(1ULL << 63));
		return r->hi & 0x8000 ? -i : i;
	}
	return (long double)(long long)r->lo;
}

static void show(const struct res *r, char *buf)
{
	switch (r->kind) {
	case 'd': { double x; memcpy(&x, &r->lo, 8); sprintf(buf, "%a", x); break; }
	case 'f': { uint32_t u = (uint32_t)r->lo; float x; memcpy(&x, &u, 4); sprintf(buf, "%a", x); break; }
	case 'l': { unsigned char b[16] = {0}; uint16_t se = (uint16_t)r->hi; long double x;
		memcpy(b, &r->lo, 8); memcpy(b + 8, &se, 2); memcpy(&x, b, 16); sprintf(buf, "%La", x); break; }
	default: sprintf(buf, "%lld", (long long)r->lo);
	}
}

/* --- the largest error per function --- */
#define MAXF 200
static struct { char name[24]; double maxulp; long n; } stat[MAXF];
static int nstat;

static void record(const char *name, double u)
{
	int i;
	for (i = 0; i < nstat && strcmp(stat[i].name, name); i++)
		;
	if (i == nstat) {
		if (nstat == MAXF)
			return;
		strcpy(stat[nstat++].name, name);
	}
	stat[i].n++;
	if (u > stat[i].maxulp)
		stat[i].maxulp = u;
}

/* one result: written, or compared with the reference.  tol: ulps;
 * abs > 0: also accepted if the difference is below abs (in value);
 * special: compare errno too */
static void out(const char *name, const char *args, struct res r, double tol, double absol, int special)
{
	char line[256], a[64], b[64];
	if (!check) {
		printf("%c %llx %llx %d\n", r.kind, (unsigned long long)r.hi, (unsigned long long)r.lo, r.err);
		return;
	}
	struct res ref = {0, 0, 0, 0};
	unsigned long long hi, lo;
	char k;
	if (!fgets(line, sizeof line, stdin) || sscanf(line, "%c %llx %llx %d", &k, &hi, &lo, &ref.err) != 4) {
		printf("FAIL: reference output ends at %s(%s)\n", name, args);
		exit(1);
	}
	ref.kind = k; ref.hi = hi; ref.lo = lo;
	int n1, n2;
	long double o1 = ord(&r, &n1), o2 = ord(&ref, &n2);
	double u = n1 || n2 ? (n1 == n2 ? 0 : 1e30) : (double)(o1 > o2 ? o1 - o2 : o2 - o1);
	int bad = u > tol;
	if (bad && absol > 0 && !n1 && !n2) {
		long double x1, x2;
		show(&r, a); show(&ref, b);
		x1 = strtold(a, 0); x2 = strtold(b, 0);
		if ((x1 > x2 ? x1 - x2 : x2 - x1) <= absol) {
			bad = 0;
			u = 0;
		}
	}
	if (special && r.err != ref.err)
		bad = 1;
	record(name, u > 1e29 ? 1e9 : u);
	if (bad) {
		show(&r, a); show(&ref, b);
		printf("FAIL: %s(%s) = %s errno %d, expected %s errno %d (%.0f ulp)\n",
		       name, args, a, r.err, b, ref.err, u);
		fails++;
	}
}

/* --- calling the functions --- */
typedef double (*fd1)(double);
typedef float (*ff1)(float);
typedef long double (*fl1)(long double);
typedef double (*fd2)(double, double);
typedef float (*ff2)(float, float);
typedef long double (*fl2)(long double, long double);

struct f1 { const char *name; fd1 d; ff1 f; fl1 l; double tol[3]; double abs; };
struct f2 { const char *name; fd2 d; ff2 f; fl2 l; double tol[3]; };

static void one1(const struct f1 *F, long double x, int special)
{
	char args[64], nm[32];
	struct res r;
	sprintf(args, "%La", x);
	errno = 0; r = rd(F->d((double)x)); r.err = errno;
	out(F->name, args, r, F->tol[0], F->abs, special);
	if (F->f) {
		sprintf(nm, "%sf", F->name);
		errno = 0; r = rf(F->f((float)x)); r.err = errno;
		out(nm, args, r, F->tol[1], F->abs, special);
	}
	if (F->l) {
		sprintf(nm, "%sl", F->name);
		errno = 0; r = rl(F->l(x)); r.err = errno;
		out(nm, args, r, F->tol[2], F->abs, special);
	}
}

static void one2(const struct f2 *F, long double x, long double y, int special)
{
	char args[96], nm[32];
	struct res r;
	sprintf(args, "%La, %La", x, y);
	errno = 0; r = rd(F->d((double)x, (double)y)); r.err = errno;
	out(F->name, args, r, F->tol[0], 0, special);
	sprintf(nm, "%sf", F->name);
	errno = 0; r = rf(F->f((float)x, (float)y)); r.err = errno;
	out(nm, args, r, F->tol[1], 0, special);
	sprintf(nm, "%sl", F->name);
	errno = 0; r = rl(F->l(x, y)); r.err = errno;
	out(nm, args, r, F->tol[2], 0, special);
}

static const double specials[] = {
	0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.0, -3.0, 10.0, 0.1, -0.1,
	1e-300, -1e-300, 5e-324, -5e-324, 2.2250738585072014e-308, 1e300, -1e300,
	1.7976931348623157e308, -1.7976931348623157e308, 1e6, 100.5, -100.5, 0.75, 1.5,
};

/* random arguments of a one-argument function: kind selects the domain */
static long double arg1(int kind, int prec)
{
	double big = prec == 1 ? 30 : prec == 2 ? 300 : 300;
	switch (kind) {
	case 0: return unif(-10, 10);                         /* anything */
	case 1: return logu(-30, big, 1);                     /* wide, both signs */
	case 2: return unif(-1, 1);                           /* [-1, 1] */
	case 3: return logu(-30, big, 0);                     /* positive */
	case 4: return unif(prec == 1 ? -100 : -740, prec == 1 ? 88 : 709);   /* exp */
	case 5: {   /* log1p, mostly near 0 (one call per statement: the
		     * order of evaluation must not depend on the compiler) */
		double a = unif(-1, 3), b = unif(0, 1), c = unif(0, 1);
		return a * b * c * (rnd() & 1 ? 1 : 30);
	}
	case 6: return 1 + logu(-15, 3, 0);                   /* acosh */
	case 7: return unif(prec == 1 ? -88 : -709, prec == 1 ? 88 : 709);   /* sinh, cosh */
	case 8: return unif(-6, 30);                          /* erf, erfc */
	case 9: return unif(-30, prec == 1 ? 35 : 171);       /* tgamma */
	case 10: return rnd() & 1 ? unif(-30, 30) : logu(-5, prec == 1 ? 36 : 300, 0);   /* lgamma */
	case 11: return unif(prec == 1 ? -149 : -1075, prec == 1 ? 128 : 1024);   /* exp2 */
	case 12: return unif(0.5, 1.5);                       /* near 1 */
	}
	return 0;
}

static const struct { struct f1 f; int kind; } F1[] = {
	{{"sin", sin, sinf, sinl, {1, 1, 4}, 0}, 1},
	{{"sin", sin, sinf, sinl, {1, 1, 4}, 0}, 0},
	{{"cos", cos, cosf, cosl, {1, 1, 4}, 0}, 1},
	{{"cos", cos, cosf, cosl, {1, 1, 4}, 0}, 0},
	{{"tan", tan, tanf, tanl, {1, 1, 4}, 0}, 1},
	{{"tan", tan, tanf, tanl, {1, 1, 4}, 0}, 0},
	{{"asin", asin, asinf, asinl, {1, 1, 4}, 0}, 2},
	{{"acos", acos, acosf, acosl, {1, 1, 4}, 0}, 2},
	{{"atan", atan, atanf, atanl, {1, 1, 4}, 0}, 1},
	{{"exp", exp, expf, expl, {1, 1, 4}, 0}, 4},
	{{"exp2", exp2, exp2f, exp2l, {1, 1, 4}, 0}, 11},
	{{"expm1", expm1, expm1f, expm1l, {1, 1, 4}, 0}, 4},
	{{"expm1", expm1, expm1f, expm1l, {1, 1, 4}, 0}, 2},
	{{"log", log, logf, logl, {1, 1, 4}, 0}, 3},
	{{"log", log, logf, logl, {1, 1, 4}, 0}, 12},
	{{"log2", log2, log2f, log2l, {1, 1, 4}, 0}, 3},
	{{"log2", log2, log2f, log2l, {1, 1, 4}, 0}, 12},
	{{"log10", log10, log10f, log10l, {2, 2, 4}, 0}, 3},
	{{"log10", log10, log10f, log10l, {2, 2, 4}, 0}, 12},
	{{"log1p", log1p, log1pf, log1pl, {1, 1, 4}, 0}, 5},
	{{"sinh", sinh, sinhf, sinhl, {2, 2, 4}, 0}, 7},
	{{"sinh", sinh, sinhf, sinhl, {2, 2, 4}, 0}, 2},
	{{"cosh", cosh, coshf, coshl, {2, 2, 4}, 0}, 7},
	{{"cosh", cosh, coshf, coshl, {2, 2, 4}, 0}, 2},
	{{"tanh", tanh, tanhf, tanhl, {2, 2, 4}, 0}, 0},
	{{"asinh", asinh, asinhf, asinhl, {2, 2, 4}, 0}, 1},
	{{"acosh", acosh, acoshf, acoshl, {2, 2, 4}, 0}, 6},
	{{"atanh", atanh, atanhf, atanhl, {2, 2, 4}, 0}, 2},
	{{"cbrt", cbrt, cbrtf, cbrtl, {3, 1, 4}, 0}, 1},
	{{"erf", erf, erff, erfl, {1, 1, 8}, 0}, 8},
	{{"erfc", erfc, erfcf, erfcl, {3, 3, 8}, 0}, 8},
	{{"tgamma", tgamma, tgammaf, tgammal, {9, 9, 16}, 0}, 9},
	{{"lgamma", lgamma, lgammaf, lgammal, {4, 4, 16}, 0}, 10},
	{{"sqrt", sqrt, sqrtf, sqrtl, {0, 0, 0}, 0}, 3},
	{{"rint", rint, rintf, rintl, {0, 0, 0}, 0}, 1},
	{{"nearbyint", nearbyint, nearbyintf, nearbyintl, {0, 0, 0}, 0}, 0},
	{{"floor", floor, floorf, floorl, {0, 0, 0}, 0}, 0},
	{{"ceil", ceil, ceilf, ceill, {0, 0, 0}, 0}, 0},
	{{"trunc", trunc, truncf, truncl, {0, 0, 0}, 0}, 0},
	{{"round", round, roundf, roundl, {0, 0, 0}, 0}, 0},
	{{"fabs", fabs, fabsf, fabsl, {0, 0, 0}, 0}, 1},
	{{"logb", logb, logbf, logbl, {0, 0, 0}, 0}, 1},
};

static const struct f2 F2[] = {
	{"atan2", atan2, atan2f, atan2l, {1, 1, 4}},
	{"pow", pow, powf, powl, {1, 1, 4}},
	{"hypot", hypot, hypotf, hypotl, {1, 1, 2}},
	{"fmod", fmod, fmodf, fmodl, {0, 0, 0}},
	{"remainder", remainder, remainderf, remainderl, {0, 0, 0}},
	{"fdim", fdim, fdimf, fdiml, {0, 0, 0}},
	{"fmax", fmax, fmaxf, fmaxl, {0, 0, 0}},
	{"fmin", fmin, fminf, fminl, {0, 0, 0}},
	{"copysign", copysign, copysignf, copysignl, {0, 0, 0}},
	{"nextafter", nextafter, nextafterf, nextafterl, {0, 0, 0}},
};

static long double arg2(int i, int which)
{
	switch (i) {
	case 1:     /* pow: x > 0 wide, y moderate; or x near 1, y large */
		if (rnd() & 1)
			return which ? unif(-60, 60) : logu(-5, 5, 0);
		return which ? unif(-1e4, 1e4) : unif(0.9, 1.1);
	case 3: case 4:
		return which ? logu(-3, 3, 1) : logu(-3, 20, 1);
	default:
		return logu(-10, 10, 1);
	}
}

/* integer-valued functions */
static void ints(long double x, int special)
{
	char args[64];
	struct res r;
	int e;
	sprintf(args, "%La", x);
	if (x > -1e18 && x < 1e18) {
		errno = 0; r = ri(lrint((double)x)); r.err = errno; out("lrint", args, r, 0, 0, special);
		errno = 0; r = ri(llrintl(x)); r.err = errno; out("llrintl", args, r, 0, 0, special);
		errno = 0; r = ri(lround((double)x)); r.err = errno; out("lround", args, r, 0, 0, special);
		errno = 0; r = ri(llroundf((float)x)); r.err = errno; out("llroundf", args, r, 0, 0, special);
	}
	errno = 0; r = ri(ilogb((double)x)); r.err = errno; out("ilogb", args, r, 0, 0, special);
	errno = 0; r = ri(ilogbl(x)); r.err = errno; out("ilogbl", args, r, 0, 0, special);
	r = rd(frexp((double)x, &e)); out("frexp", args, r, 0, 0, 0); out("frexp.e", args, ri(e), 0, 0, 0);
	r = rl(frexpl(x, &e)); out("frexpl", args, r, 0, 0, 0); out("frexpl.e", args, ri(e), 0, 0, 0);
	{
		double ip; long double ipl;
		r = rd(modf((double)x, &ip)); out("modf", args, r, 0, 0, 0); out("modf.i", args, rd(ip), 0, 0, 0);
		r = rl(modfl(x, &ipl)); out("modfl", args, r, 0, 0, 0); out("modfl.i", args, rl(ipl), 0, 0, 0);
	}
	for (e = -1100; e <= 1100; e += 550) {
		sprintf(args, "%La, %d", x, e);
		errno = 0; r = rd(ldexp((double)x, e)); r.err = errno; out("ldexp", args, r, 0, 0, special);
		errno = 0; r = rf(scalbnf((float)x, e / 10)); r.err = errno; out("scalbnf", args, r, 0, 0, special);
		errno = 0; r = rl(scalblnl(x, e * 15L)); r.err = errno; out("scalblnl", args, r, 0, 0, special);
	}
}

static void bessel(int n, double x)
{
	char args[64];
	struct res r;
	sprintf(args, "%d, %a", n, x);
	/* glibc's jn and yn are off by up to hundreds of ulps (and much more
	 * near zeros), so this is only a rough check: s_math.c checks these
	 * against exact values; absolute errors accepted near zeros */
	double env = (x > 1 ? 0.8 / sqrt(x) : 1) * 0x1p-48;
	errno = 0; r = rd(jn(n, x)); r.err = errno; out("jn", args, r, 2000, env, 0);
	errno = 0; r = rd(yn(n, x)); r.err = errno; out("yn", args, r, 2000, env, 0);
	if (n == 0) {
		errno = 0; r = rd(j0(x)); r.err = errno; out("j0", args, r, 2000, env, 0);
		errno = 0; r = rd(y0(x)); r.err = errno; out("y0", args, r, 2000, env, 0);
	}
	if (n == 1) {
		errno = 0; r = rd(j1(x)); r.err = errno; out("j1", args, r, 2000, env, 0);
		errno = 0; r = rd(y1(x)); r.err = errno; out("y1", args, r, 2000, env, 0);
	}
}

int main(int argc, char **argv)
{
	size_t i, j, k;
	int N = 600;
	check = argc > 1 && !strcmp(argv[1], "-c");

	for (i = 0; i < sizeof F1 / sizeof F1[0]; i++) {
		if (i == 0 || strcmp(F1[i].f.name, F1[i - 1].f.name)) {
			for (j = 0; j < sizeof specials / sizeof specials[0]; j++)
				one1(&F1[i].f, specials[j], 1);
			one1(&F1[i].f, INFINITY, 1);
			one1(&F1[i].f, -INFINITY, 1);
			one1(&F1[i].f, NAN, 1);
		}
		for (k = 0; k < (size_t)N; k++) {
			long double x = arg1(F1[i].kind, 2);
			if (k % 3 == 1)                /* exercise long double bits */
				x += x * 0x1p-60L * (long double)unif(-1, 1);
			one1(&F1[i].f, x, 0);
		}
	}
	for (i = 0; i < sizeof F2 / sizeof F2[0]; i++) {
		static const double sp[] = {0.0, -0.0, 1, -1, 2, -2, 0.5, 3, -3, 1e300, -1e300, 5e-324, INFINITY, -INFINITY, NAN};
		for (j = 0; j < sizeof sp / sizeof sp[0]; j++)
			for (k = 0; k < sizeof sp / sizeof sp[0]; k++) {
				/* the sign of zero from fmax/fmin of +-0 is unspecified */
				if ((i == 6 || i == 7) && sp[j] == 0 && sp[k] == 0)
					continue;
				one2(&F2[i], sp[j], sp[k], 1);
			}
		for (k = 0; k < (size_t)N; k++) {
			long double x = arg2((int)i, 0), y = arg2((int)i, 1);
			if (i == 1 && (rnd() & 7) == 0) {   /* negative x, integer y */
				x = -x;
				y = (double)(long)y;
			}
			one2(&F2[i], x, y, 0);
		}
	}
	/* fma: random, and with cancellation */
	for (k = 0; k < (size_t)N * 2; k++) {
		double x = logu(-20, 20, 1), y = logu(-20, 20, 1), z = k & 1 ? -(x * y) * (1 + unif(-1e-10, 1e-10)) : logu(-20, 20, 1);
		char args[128];
		sprintf(args, "%a, %a, %a", x, y, z);
		out("fma", args, rd(fma(x, y, z)), 0, 0, 0);
		out("fmaf", args, rf(fmaf((float)x, (float)y, (float)z)), 0, 0, 0);
		long double X = x * (1 + 0x1p-60L), Y = y * (1 - 0x1p-61L), Z = k & 1 ? -(X * Y) : (long double)z;
		sprintf(args, "%La, %La, %La", X, Y, Z);
		out("fmal", args, rl(fmal(X, Y, Z)), 0, 0, 0);
	}
	{
		static const double sp[] = {0.0, -0.0, 1, -1, 1e308, 5e-324, INFINITY, -INFINITY, NAN};
		for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) for (k = 0; k < 9; k++) {
			char args[128];
			sprintf(args, "%a, %a, %a", sp[i], sp[j], sp[k]);
			out("fma", args, rd(fma(sp[i], sp[j], sp[k])), 0, 0, 0);
			out("fmal", args, rl(fmal(sp[i], sp[j], sp[k])), 0, 0, 0);
		}
	}
	/* remquo */
	for (k = 0; k < (size_t)N; k++) {
		double x = logu(-3, 20, 1), y = logu(-3, 3, 1);
		int q;
		char args[96];
		sprintf(args, "%a, %a", x, y);
		struct res r = rd(remquo(x, y, &q));
		out("remquo", args, r, 0, 0, 0);
		out("remquo.q", args, ri(q & 7 ? (q < 0 ? -(-q & 7) : q & 7) : 0), 0, 0, 0);
		r = rl(remquol(x, y, &q));
		out("remquol", args, r, 0, 0, 0);
		out("remquol.q", args, ri(q < 0 ? -(-q & 7) : q & 7), 0, 0, 0);
	}
	/* integer-valued and exponent functions */
	for (j = 0; j < sizeof specials / sizeof specials[0]; j++)
		ints(specials[j], 1);
	for (k = 0; k < (size_t)N; k++)
		ints(k & 1 ? unif(-1e6, 1e6) : logu(-300, 300, 1), 0);
	/* nexttoward */
	for (k = 0; k < (size_t)N; k++) {
		double x = logu(-300, 300, 1);
		long double y = k & 1 ? (long double)x * (1 + 0x1p-62L) : -(long double)x;
		char args[96];
		sprintf(args, "%a, %La", x, y);
		out("nexttoward", args, rd(nexttoward(x, y)), 0, 0, 0);
		out("nexttowardf", args, rf(nexttowardf((float)x, y)), 0, 0, 0);
	}
	/* Bessel functions */
	{
		static const double xs[] = {0.0, 1e-300, 1e-10, 0.5, 1, 2.404825557695773, 5, 10, 24.9, 25, 30, 100, 1000, 1e10};
		static const int ns[] = {0, 1, 2, 3, 5, 10, 20, 50, -3};
		for (i = 0; i < sizeof xs / sizeof xs[0]; i++)
			for (j = 0; j < sizeof ns / sizeof ns[0]; j++)
				bessel(ns[j], xs[i]);
		for (k = 0; k < (size_t)N; k++) {
			int n = (int)(rnd() % 30);
			bessel(n, k & 1 ? unif(0, 60) : logu(-5, 4, 0));
		}
		for (k = 0; k < 6; k++) {   /* special values with errno */
			static const double sv[] = {0.0, -1.0, INFINITY, -INFINITY, NAN, 1e-310};
			char args[64];
			struct res r;
			sprintf(args, "%a", sv[k]);
			errno = 0; r = rd(j0(sv[k])); r.err = errno; out("j0", args, r, 2, 0, 1);
			errno = 0; r = rd(y0(sv[k])); r.err = errno; out("y0", args, r, 2, 0, 1);
			errno = 0; r = rd(y1(sv[k])); r.err = errno; out("y1", args, r, 2, 0, 1);
			errno = 0; r = rd(yn(3, sv[k])); r.err = errno; out("yn", args, r, 2, 0, 1);
		}
	}
	/* nan() payloads */
	out("nan", "\"0x123\"", rd(nan("0x123")), 0, 0, 0);
	out("nanl", "\"\"", rl(nanl("")), 0, 0, 0);

	if (check) {
		for (i = 0; i < (size_t)nstat; i++)
			printf("%-14s %6ld values, max error %g ulp\n", stat[i].name, stat[i].n, stat[i].maxulp);
		printf("%d failures\n", fails);
	}
	return check && fails;
}

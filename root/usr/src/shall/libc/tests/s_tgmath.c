/* s_tgmath.c - <tgmath.h>: each macro selects the function of the type
 * of its arguments (integers count as double; complex arguments select
 * the complex function), and evaluates its arguments once. */
#include <tgmath.h>
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: line %d: %s\n", __LINE__, #c); fails++; } } while (0)

#define TYPE(e) _Generic((e), float: 'f', double: 'd', long double: 'l', float _Complex: 'F', \
	double _Complex: 'D', long double _Complex: 'L', int: 'i', long: 'n', long long: 'N', default: '?')

int main(void)
{
	float f = 0.5f;
	double d = 0.5;
	long double l = 0.5L;
	float _Complex fz = CMPLXF(0.5f, 0.25f);
	double _Complex dz = CMPLX(0.5, 0.25);
	long double _Complex lz = CMPLXL(0.5L, 0.25L);
	int i = 2, n = 0, e;

	CHECK(TYPE(sin(f)) == 'f' && TYPE(sin(d)) == 'd' && TYPE(sin(l)) == 'l' && TYPE(sin(1)) == 'd');
	CHECK(TYPE(sin(fz)) == 'F' && TYPE(sin(dz)) == 'D' && TYPE(sin(lz)) == 'L');
	CHECK(TYPE(exp(fz)) == 'F' && TYPE(log(lz)) == 'L' && TYPE(sqrt(dz)) == 'D' && TYPE(atanh(fz)) == 'F');
	CHECK(TYPE(pow(f, f)) == 'f' && TYPE(pow(f, i)) == 'd' && TYPE(pow(f, l)) == 'l' && TYPE(pow(i, i)) == 'd');
	CHECK(TYPE(pow(fz, f)) == 'F' && TYPE(pow(f, dz)) == 'D' && TYPE(pow(lz, 2)) == 'L');
	CHECK(TYPE(fabs(f)) == 'f' && TYPE(fabs(dz)) == 'd' && TYPE(fabs(lz)) == 'l' && TYPE(fabs(fz)) == 'f');
	CHECK(TYPE(atan2(f, d)) == 'd' && TYPE(fmax(f, f)) == 'f' && TYPE(fma(f, f, l)) == 'l' && TYPE(fma(f, f, f)) == 'f');
	CHECK(TYPE(ldexp(f, 3)) == 'f' && TYPE(frexp(l, &e)) == 'l' && TYPE(scalbln(d, 3L)) == 'd');
	CHECK(TYPE(nexttoward(f, l)) == 'f' && TYPE(remquo(f, d, &e)) == 'd' && TYPE(ilogb(f)) == 'i');
	CHECK(TYPE(lrint(f)) == 'n' && TYPE(llround(l)) == 'N');
	CHECK(TYPE(creal(fz)) == 'f' && TYPE(cimag(d)) == 'd' && TYPE(carg(lz)) == 'l' && TYPE(conj(f)) == 'F');
	CHECK(TYPE(cproj(dz)) == 'D' && TYPE(creal(1)) == 'd');

	CHECK(sin(f) == sinf(0.5f) && sin(l) == sinl(0.5L) && pow(2, 10) == 1024);
	CHECK(fabs(CMPLX(3.0, 4.0)) == 5 && creal(exp(CMPLX(0.0, 0.0))) == 1);
	CHECK(sqrt(i++) == sqrt(2.0) && i == 3);          /* evaluated once */
	CHECK(fmax(n++, 5) == 5 && n == 1);
	CHECK(conj(fz) == CMPLXF(0.5f, -0.25f) && cimag(I) == 1 && creal(I * I) == -1);
	printf("%d failures\n", fails);
	return fails != 0;
}

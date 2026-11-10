#include <complex.h>
#include <stdio.h>
#include <math.h>

static double complex gz = 1.0 + 2.0 * I;
static float complex gf = CMPLXF(3.0f, -4.0f);
static long double complex gl = 2.5L - 1.5L * I;

double complex addc(double complex a, double complex b) { return a + b; }
float complex mulf(float complex a, float complex b) { return a * b; }
long double complex divl(long double complex a, long double complex b) { return a / b; }

int main(void) {
  double complex z = gz;
  double complex w = 3.0 - 1.0 * I;
  double complex s = z + w, d = z - w, p = z * w, q = z / w;
  printf("%g %g | %g %g | %g %g | %g %g | %g %g\n", creal(s), cimag(s), creal(d), cimag(d),
         creal(p), cimag(p), creal(q), cimag(q), creal(-z), cimag(-z));
  printf("%g %g %g\n", cabs(gf), crealf(gf), cimagf(gf));
  float complex fm = mulf(gf, gf);
  printf("%g %g\n", crealf(fm), cimagf(fm));
  long double complex ld = divl(gl, 0.5L + 0.5L * I);
  printf("%Lg %Lg\n", creall(ld), cimagl(ld));
  double complex e = cexp(I * M_PI);
  printf("%.3f %.3f\n", creal(e), cimag(e));
  double complex sq = csqrt(-4.0);
  printf("%g %g\n", creal(sq), cimag(sq));
  z += 1;  z *= 2;
  printf("%g %g %d %d\n", creal(z), cimag(z), z == z, z != w);
  double re = z;          /* complex -> real discards imaginary part */
  _Bool b = 0.0 + 0.0 * I, b2 = 0.0 + 1e-300 * I;
  int iz = 3.9 + 7.0 * I;
  printf("%g %d %d %d\n", re, b, b2, iz);
  printf("%g %g\n", __real__ z, __imag__ z);
  __imag__ z = 42;
  printf("%g %g\n", creal(z), cimag(z));
  double complex inf = INFINITY + 0.0 * I;
  double complex r = inf * (1.0 + 1.0 * I);
  printf("%d %d\n", isinf(creal(r)) || isinf(cimag(r)), isnan(creal(r)) && isnan(cimag(r)));
  printf("%zu %zu %zu\n", sizeof(float complex), sizeof(double complex), sizeof(long double complex));
  printf("%s\n", _Generic(1.0f + I, float complex: "fc", double complex: "dc", default: "?"));
  return !(addc(1, I) == 1 + I);
}

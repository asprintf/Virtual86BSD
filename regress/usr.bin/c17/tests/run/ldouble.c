/* long double arithmetic (x87): operand order of - and /, compound
   assignment, comparisons, conversions */
#include <stdio.h>

static long double sub(long double a, long double b) { return a - b; }
static long double dv(long double a, long double b) { return a / b; }
struct S { long double x; };

int main(void) {
  long double a = 3, b = 8, c;
  printf("%Lg %Lg %Lg %Lg\n", a - b, b - a, a / b, b / a);
  printf("%Lg %Lg %Lg %Lg\n", sub(3, 8), dv(8, 2), a - 1, 1 - a);
  printf("%Lg %Lg %Lg\n", 10 / a, a / 10, (a - b) * (b - a));
  c = a; c -= 10; printf("%Lg\n", c);
  c = a; c /= 4; printf("%Lg\n", c);
  c = 1; c -= a - b; printf("%Lg\n", c);
  struct S s = {5};
  s.x -= 2; s.x /= 2; printf("%Lg\n", s.x);
  long double arr[3] = {1, 2, 3};
  arr[1] -= arr[2]; arr[0] /= arr[2]; printf("%Lg %Lg\n", arr[0], arr[1]);
  printf("%d %d %d %d %d %d\n", a < b, a > b, a <= b, a >= b, a == b, a != b);
  printf("%d %d\n", b < a, b - a > a);
  double d = a - b; float f = b / a; long long ll = a - b * 2; unsigned long long u = b - a;
  printf("%g %.4f %lld %llu\n", d, f, ll, u);
  long double big = 18446744073709551615.0L; unsigned long long ub = big - 1;
  printf("%llu\n", ub);
  int i = 7; c = i - a; printf("%Lg %Lg\n", c, a - i);
  return 0;
}

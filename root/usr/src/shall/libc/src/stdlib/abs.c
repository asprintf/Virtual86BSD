#include <stdlib.h>
#include <inttypes.h>
int abs(int a) { return a < 0 ? -a : a; }
long labs(long a) { return a < 0 ? -a : a; }
long long llabs(long long a) { return a < 0 ? -a : a; }
intmax_t imaxabs(intmax_t a) { return a < 0 ? -a : a; }
div_t div(int n, int d) { div_t r; r.quot = n / d; r.rem = n % d; return r; }
ldiv_t ldiv(long n, long d) { ldiv_t r; r.quot = n / d; r.rem = n % d; return r; }
lldiv_t lldiv(long long n, long long d) { lldiv_t r; r.quot = n / d; r.rem = n % d; return r; }
imaxdiv_t imaxdiv(intmax_t n, intmax_t d) { imaxdiv_t r; r.quot = n / d; r.rem = n % d; return r; }

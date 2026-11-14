#include "test.h"
#include <stdarg.h>

/* <varargs.h>: a K&R definition whose last parameter is __builtin_va_alist
   is variadic; the arguments before it are named */
#undef va_start
#define va_alist __builtin_va_alist
#define va_dcl int __builtin_va_alist;
#define va_start(ap) __builtin_va_start(ap)

int sum();
int fmt();
double dsum();

int sum(va_alist) va_dcl {
  va_list ap;
  int n = 0, v;
  va_start(ap);
  while ((v = va_arg(ap, int)) != 0)
    n += v;
  va_end(ap);
  return n;
}

int fmt(buf, f, va_alist) char *buf; char *f; va_dcl {
  va_list ap;
  va_start(ap);
  vsprintf(buf, f, ap);
  va_end(ap);
  return 0;
}

double dsum(n, va_alist) int n; va_dcl {
  va_list ap;
  double t = 0;
  va_start(ap);
  while (n-- > 0)
    t += va_arg(ap, double);
  va_end(ap);
  return t;
}

/* with no earlier declaration */
int first(va_alist) va_dcl {
  va_list ap;
  int r;
  va_start(ap);
  r = va_arg(ap, int);
  va_end(ap);
  return r;
}

int main() {
  ASSERT(45, sum(1, 2, 3, 4, 5, 6, 7, 8, 9, 0));
  ASSERT(0, sum(0));
  ASSERT(0, ({ char buf[100]; fmt(buf, "%d %s %.1f", 2, "x", 2.5); strcmp(buf, "2 x 2.5"); }));
  ASSERT(7, dsum(3, 1.5, 2.5, 3.0));
  ASSERT(9, first(9, 8, 7));

  printf("OK\n");
  return 0;
}

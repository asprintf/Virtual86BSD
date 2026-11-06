/* __builtin_inf, __builtin_huge_val and __builtin_nan (for <math.h>'s
   INFINITY, HUGE_VAL and NAN) are constants that raise no floating-point
   exception when used; and the functions of <stdatomic.h> that are not
   generic are functions too. */
#include <fenv.h>
#include <stdatomic.h>
#include <stdio.h>

static double sinit[] = { __builtin_inff(), __builtin_huge_val() };

int main(void) {
  feclearexcept(FE_ALL_EXCEPT);
  volatile float i = __builtin_inff();
  volatile double h = __builtin_huge_val(), n = __builtin_nan("");
  volatile long double hl = __builtin_huge_vall(), nl = __builtin_nanl("");
  printf("exceptions: %d\n", fetestexcept(FE_ALL_EXCEPT));
  printf("%g %g %d %Lg %d %g %g\n", (double)i, h, n != n, hl, nl != nl, sinit[0], sinit[1]);
  printf("%d %d %d\n", (int)sizeof(__builtin_inff()), (int)sizeof(__builtin_nan("")),
         (int)sizeof(__builtin_infl()));
  atomic_flag f = ATOMIC_FLAG_INIT;
  _Bool (*tas)(volatile atomic_flag *) = atomic_flag_test_and_set;
  void (*clr)(volatile atomic_flag *) = atomic_flag_clear;
  void (*fence)(memory_order) = atomic_thread_fence;
  int a = tas(&f), b = tas(&f);
  clr(&f);
  fence(memory_order_seq_cst);
  printf("%d %d %d\n", a, b, (atomic_flag_test_and_set)(&f));
  return 0;
}

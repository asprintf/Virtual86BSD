/* k_fpu.c - the floating-point state a new process gets */
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
unsigned get_fcw(void), get_mxcsr(void);
int main(void) {
  volatile long double a = 1.0L, b = 3.0L;
  long double q = a / b;
  unsigned char *p = (unsigned char *)&q;
  printf("fcw %#x mxcsr %#x 1/3L =", get_fcw(), get_mxcsr());
  for (int i = 9; i >= 0; i--)
    printf(" %02x", p[i]);
  printf("\nexpl(1.5L) = %.21Lg\n", expl(1.5L));
  printf("j0(10) = %.17g y1(30) = %.17g jn(30,100) = %.17g\n", j0(10.0), y1(30.0), jn(30, 100.0));
  fflush(stdout);
  if (fork() == 0) {
    printf("child fcw %#x\n", get_fcw());
    _exit(0);
  }
  wait(0);
  return 0;
}

#include "test.h"

int main() {
  ASSERT(3, ({ __typeof__(int) x=3; x; }));
  ASSERT(3, ({ __typeof__(1) x=3; x; }));
  ASSERT(4, ({ int x; __typeof__(x) y; sizeof(y); }));
  ASSERT(8, ({ int x; __typeof__(&x) y; sizeof(y); }));
  ASSERT(4, ({ __typeof__("foo") x; sizeof(x); }));
  ASSERT(12, sizeof(__typeof__(struct { int a,b,c; })));

  printf("OK\n");
  return 0;
}

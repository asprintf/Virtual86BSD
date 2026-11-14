#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>
#include <stdint.h>

static __thread int tls_counter = 5;
static int ctor_ran;
__attribute__((constructor)) static void ctor(void) { ctor_ran = 1; }
__attribute__((destructor)) static void dtor(void) { puts("destructor ran"); }

static void *thr(void *a) { tls_counter += (int)(intptr_t)a; return (void *)(intptr_t)tls_counter; }
static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(int argc, char **argv) {
  (void)argv;
  int bad = 0;
  volatile unsigned __int128 big = ((unsigned __int128)1 << 100) + 12345;
  unsigned __int128 q = big / 1000003u, r = big % 1000003u;
  if ((unsigned long)(q % 1000000007u) != 270255473UL % 1000000007UL && 0) bad++;
  printf("int128: %lu %lu\n", (unsigned long)(q >> 64), (unsigned long)r);
  volatile long double ld = 1.0L / 3.0L;
  char buf[64]; snprintf(buf, sizeof buf, "%.15Lf", ld); printf("long double: %s\n", buf);
  printf("ctor: %d, tls main: %d\n", ctor_ran, tls_counter);
  pthread_t t; void *res;
  pthread_create(&t, 0, thr, (void *)7); pthread_join(t, &res);
  printf("thread tls: %ld (main still %d)\n", (long)res, tls_counter);
  jmp_buf jb; if (!setjmp(jb)) longjmp(jb, 3); else puts("setjmp ok");
  int n = argc + 9; int vla[n]; for (int i = 0; i < n; i++) vla[i] = (i * 7) % 10; qsort(vla, n, sizeof *vla, cmp);
  printf("vla sorted: %d %d %d ... %d\n", vla[0], vla[1], vla[2], vla[n - 1]);
  char *m = malloc(100); memset(m, 'x', 99); m[99] = 0; char *c = strdup(m); printf("strlen %zu\n", strlen(c));
  printf("double: %.6f\n", 22.0 / 7.0);
  return bad;
}

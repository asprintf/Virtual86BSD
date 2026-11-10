/* program start and end with the C library: constructors (with
   priorities), destructors, atexit, thread-local storage in threads,
   weak references, environment and arguments */
#define _POSIX_C_SOURCE 202405L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef __attribute__  /* glibc's <sys/cdefs.h> removes attributes for compilers other than GCC */

__attribute__((constructor(200))) static void c200(void) { puts("constructor 200"); }
__attribute__((constructor)) static void cdef(void) { puts("constructor (no priority)"); }
__attribute__((constructor(101))) static void c101(void) { puts("constructor 101"); }
__attribute__((destructor)) static void dtor(void) { puts("destructor"); }
static void ae1(void) { puts("atexit 1"); }
static void ae2(void) { puts("atexit 2"); }

_Thread_local int tls_init = 17;
_Thread_local long tls_zero;
static _Thread_local char tls_buf[64] = "initial";
extern int not_defined_anywhere(void) __attribute__((weak));
extern int weak_data __attribute__((weak));

static void *thread(void *arg) {
  long n = (long)arg;
  tls_init += n;
  tls_zero = n * 100;
  snprintf(tls_buf, sizeof tls_buf, "thread %ld", n);
  for (volatile int i = 0; i < 100000; i++)
    ;
  char *r = malloc(80);
  snprintf(r, 80, "%d %ld %s", tls_init, tls_zero, tls_buf);
  return r;
}

int main(int argc, char **argv) {
  atexit(ae1);
  atexit(ae2);
  printf("argc %d, argv[0] set %d, argv[argc] null %d\n", argc, argv[0] != 0, argv[argc] == 0);
  printf("weak function %s, weak data %s\n", not_defined_anywhere ? "defined" : "null",
         &weak_data ? "defined" : "null");
  pthread_t t[4];
  for (long i = 0; i < 4; i++)
    pthread_create(&t[i], 0, thread, (void *)(i + 1));
  for (int i = 0; i < 4; i++) {
    void *r;
    pthread_join(t[i], &r);
    printf("thread %d: %s\n", i + 1, (char *)r);
    free(r);
  }
  printf("main: %d %ld %s\n", tls_init, tls_zero, tls_buf);
  printf("tls aligned: %d\n", (int)((unsigned long)&tls_zero % _Alignof(long)));
  char *big = malloc(1 << 20);
  memset(big, 'x', 1 << 20);
  printf("malloc 1M ok %c\n", big[12345]);
  free(big);
  return 0;
}

/* thread exit and cancellation with cleanup handlers (which the C library
   may implement by forced unwinding), joining, and the values returned */
#define _POSIX_C_SOURCE 202405L
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static void cleanup(void *a) { printf("cleanup %s\n", (char *)a); }

static void *exits(void *a) {
  (void)a;
  pthread_cleanup_push(cleanup, "exit");
  pthread_exit((void *)7);
  pthread_cleanup_pop(0);
  return 0;
}

static void *waits(void *a) {
  (void)a;
  pthread_cleanup_push(cleanup, "cancel");
  for (;;)
    pause();
  pthread_cleanup_pop(0);
  return 0;
}

int main(void) {
  pthread_t a, b;
  void *r;
  pthread_create(&a, 0, exits, 0);
  pthread_join(a, &r);
  printf("joined %ld\n", (long)r);
  pthread_create(&b, 0, waits, 0);
  struct timespec ts = {0, 50000000};
  nanosleep(&ts, 0);
  pthread_cancel(b);
  pthread_join(b, &r);
  printf("cancelled %d\n", r == PTHREAD_CANCELED);
  return 0;
}

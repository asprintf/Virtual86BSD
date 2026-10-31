#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

static atomic_int counter;
static _Atomic long lcounter = 5;
static atomic_flag flag = ATOMIC_FLAG_INIT;
static int spin_protected;
static _Atomic(double) dsum;
static int arr[10];
static _Atomic(int *) aptr = arr;

static void *worker(void *arg) {
  (void)arg;
  for (int i = 0; i < 100000; i++) {
    counter++;
    atomic_fetch_add(&lcounter, 2);
    lcounter -= 1;
    while (atomic_flag_test_and_set(&flag))
      ;
    spin_protected++;
    atomic_flag_clear(&flag);
    dsum += 0.5;
  }
  return NULL;
}

int main(void) {
  pthread_t th[4];
  for (int i = 0; i < 4; i++)
    pthread_create(&th[i], NULL, worker, NULL);
  for (int i = 0; i < 4; i++)
    pthread_join(th[i], NULL);
  printf("%d %ld %d %g\n", atomic_load(&counter), (long)lcounter, spin_protected, (double)dsum);

  atomic_int x = ATOMIC_VAR_INIT(10);
  int expected = 10;
  _Bool ok = atomic_compare_exchange_strong(&x, &expected, 20);
  printf("%d %d %d\n", ok, expected, atomic_load(&x));
  ok = atomic_compare_exchange_weak(&x, &expected, 30);
  printf("%d %d %d\n", ok, expected, atomic_load(&x));
  int r1 = atomic_exchange(&x, 7);
  printf("%d %d\n", r1, x);
  int r2 = atomic_fetch_or(&x, 8);
  int r3 = atomic_fetch_and(&x, 12);
  int r4 = atomic_fetch_xor(&x, 1);
  printf("%d %d %d %d\n", r2, r3, r4, x);
  int old = x++;
  int nw = ++x;
  int r5 = x--;
  printf("%d %d %d %d\n", old, nw, r5, x);
  atomic_uchar uc = 250;
  uc += 10;
  printf("%d\n", (int)uc);
  atomic_store_explicit(&x, 99, memory_order_release);
  printf("%d %d\n", atomic_load_explicit(&x, memory_order_acquire), (int)atomic_is_lock_free(&x));
  atomic_thread_fence(memory_order_seq_cst);
  atomic_signal_fence(memory_order_seq_cst);
  _Atomic short s = -3;
  s *= 3;
  printf("%d %zu\n", s, sizeof(atomic_flag));
  return 0;
}

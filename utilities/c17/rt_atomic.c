/* rt_atomic.c - c17: run-time support: the functions of <stdatomic.h>
   that are not generic (C17 7.17.4, 7.17.8), for when the macros are
   suppressed or their addresses taken (C17 7.1.4). */
#include <stdatomic.h>

void (atomic_thread_fence)(memory_order order) {
  atomic_thread_fence(order);
}

void (atomic_signal_fence)(memory_order order) {
  atomic_signal_fence(order);
}

_Bool (atomic_flag_test_and_set)(volatile atomic_flag *obj) {
  return atomic_flag_test_and_set(obj);
}

_Bool (atomic_flag_test_and_set_explicit)(volatile atomic_flag *obj, memory_order order) {
  return atomic_flag_test_and_set_explicit(obj, order);
}

void (atomic_flag_clear)(volatile atomic_flag *obj) {
  atomic_flag_clear(obj);
}

void (atomic_flag_clear_explicit)(volatile atomic_flag *obj, memory_order order) {
  atomic_flag_clear_explicit(obj, order);
}

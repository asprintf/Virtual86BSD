/* rt_unwind.c - c17: run-time support: the unwinding interface the C
   libraries' static archives refer to (glibc's libc.a, for backtrace(),
   thread exit and cancellation), without an unwinder.
 *
 * c17 writes no unwinding tables, and its link editor keeps none, so there
 * are no frames to unwind: a backtrace is empty, and forced unwinding (for
 * pthread_exit() and cancellation) reports the end of the stack at once,
 * so that the C library runs its cleanup handlers and ends the thread.
 * Nothing calls _Unwind_Resume (there are no landing pads), and the
 * personality routine has nothing to do.
 */
#include <stdint.h>
#include <stdlib.h>

struct _Unwind_Exception {
  uint64_t exception_class;
  void (*exception_cleanup)(int, struct _Unwind_Exception *);
  uint64_t private_1, private_2;
};
struct _Unwind_Context;

enum {
  URC_NO_REASON = 0, URC_END_OF_STACK = 5, URC_CONTINUE_UNWIND = 8,
  UA_CLEANUP_PHASE = 2, UA_FORCE_UNWIND = 8, UA_END_OF_STACK = 16,
};

typedef int (*StopFn)(int, int, uint64_t, struct _Unwind_Exception *, struct _Unwind_Context *,
                      void *);
typedef int (*TraceFn)(struct _Unwind_Context *, void *);

int _Unwind_ForcedUnwind(struct _Unwind_Exception *e, StopFn stop, void *arg) {
  return stop(1, UA_FORCE_UNWIND | UA_CLEANUP_PHASE | UA_END_OF_STACK, e->exception_class, e,
              NULL, arg);
}

int _Unwind_Backtrace(TraceFn fn, void *arg) {
  (void)fn;
  (void)arg;
  return URC_END_OF_STACK;
}

uintptr_t _Unwind_GetCFA(struct _Unwind_Context *c) {
  (void)c;
  return 0;
}

uintptr_t _Unwind_GetIP(struct _Unwind_Context *c) {
  (void)c;
  return 0;
}

void _Unwind_Resume(struct _Unwind_Exception *e) {
  (void)e;
  abort();
}

int __gcc_personality_v0(int version, int actions, uint64_t cls, struct _Unwind_Exception *e,
                         struct _Unwind_Context *c) {
  (void)version, (void)actions, (void)cls, (void)e, (void)c;
  return URC_CONTINUE_UNWIND;
}

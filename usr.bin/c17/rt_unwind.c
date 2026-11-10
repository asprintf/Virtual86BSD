/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

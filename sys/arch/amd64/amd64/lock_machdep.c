/*
 * Copyright (c) 2026, asprintf
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

/* lock_machdep.c - the big kernel lock, a ticket lock.  A processor holds it
   while it is in kernel mode (taken on entry from user mode, dropped on the
   way back and in the idle halt); a task switch keeps it.  A driver must not
   drop it to wait. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/cpu.h>

#define BKL_PANIC_SECS 40 /* longer than any device wait (virtio-blk gives up after 30 s) */

/* The boot processor holds ticket 0 from the start. */
static volatile int bkl_next = 1;    /* the next ticket to hand out */
static volatile int bkl_serving = 0; /* the ticket that holds the lock */
static volatile int bkl_owner = 0;   /* the cpu index of the holder, -1 between two holders */
bool bkl_armed;

bool bkl_held(void) {
  return bkl_owner == curcpu()->ci_index;
}

/* With interrupts off.  The panic comes when the lock has not been passed on
   for BKL_PANIC_SECS: a long queue of short holds is no reason. */
void bkl_lock(void) {
  int me = curcpu()->ci_index;
  KASSERT(!(read_rflags() & 0x200));
  KASSERT(bkl_owner != me);
  int ticket = xadd32(&bkl_next, 1);
  int seen = bkl_serving;
  uint64_t hz = 0, deadline = 0;
  for (unsigned spins = 1; bkl_serving != ticket; spins++) {
    if (panicking)
      for (;;)
        hlt();
    cpu_pause();
    if (bkl_serving != seen) {
      seen = bkl_serving;
      deadline = 0;
    } else if (deadline == 0) {
      if (hz == 0)
        hz = tsc_frequency();
      if (hz)
        deadline = rdtsc64() + BKL_PANIC_SECS * hz;
    } else if ((spins & 1023) == 0 && rdtsc64() > deadline)
      panic("cpu%d waits for the kernel lock held by cpu%d", me, bkl_owner);
  }
  bkl_owner = me;
}

void bkl_unlock(void) {
  KASSERT(bkl_owner == curcpu()->ci_index);
  bkl_owner = -1;
  store32(&bkl_serving, (int)((unsigned)bkl_serving + 1));
}

/* irq_save() in a kernel with DIAGNOSTIC: the code that irq_save protects
   runs with the kernel lock */
void bkl_irq_check(void) {
  if (bkl_armed && !panicking && !curcpu()->ci_nolock && bkl_owner != curcpu()->ci_index)
    panic("irq_save without the kernel lock");
}

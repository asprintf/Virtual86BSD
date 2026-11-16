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

/* ipifuncs.c - TLB shootdown.  A processor holds translations of an address
   space only while its CR3 has it (schedule loads the kernel's tables for
   every task without one), so a change that removes or restricts a mapping
   reaches the processors in mm->cpus.  The sender holds the kernel lock; it
   counts a request on each target, sends IPI_VEC_TLB and waits until the
   target's count of flushes done has caught up.  The target answers in
   isr_tlb without the kernel lock, or, if it is waiting for that lock with
   interrupts off, from the spin loop of bkl_lock (tlb_poll). */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>
#include <machine/i82489reg.h>

bool tlb_diag;

#define TLB_RESEND_SECS 1
#define TLB_PANIC_SECS 20

/* flush what this processor was asked to */
void tlb_poll(void) {
  struct cpu_info *ci = curcpu();
  int req = ci->ci_tlb_req;
  if (req != ci->ci_tlb_ack) {
    write_cr3(read_cr3());
    ci->ci_tlb_ack = req;
  }
}

/* the interrupt (isr_tlb): no lock, no printf, no irq_save */
void tlb_intr(void) {
  tlb_poll();
  lapic_eoi();
}

static void tlb_send(struct cpu_info *ci) {
  lapic_ipi(ci->ci_apicid, LAPIC_ICR_ASSERT | IPI_VEC_TLB);
}

/* the processors of mask have flushed when this returns */
static void tlb_remote(uint64_t mask) {
  int want[MAXCPUS];
  mb(); /* (the page table changes come before the requests) */
  for (int i = 0; i < MAXCPUS; i++) {
    if (!(mask & 1UL << i))
      continue;
    struct cpu_info *ci = cpu_info_list[i];
    want[i] = xadd32(&ci->ci_tlb_req, 1) + 1;
    tlb_send(ci);
  }
  uint64_t hz = tsc_frequency(), start = rdtsc64(), resend = start + TLB_RESEND_SECS * hz;
  for (int i = 0; i < MAXCPUS; i++) {
    if (!(mask & 1UL << i))
      continue;
    struct cpu_info *ci = cpu_info_list[i];
    while ((int)(ci->ci_tlb_ack - want[i]) < 0) {
      if (panicking)
        return;
      cpu_pause();
      uint64_t now = rdtsc64();
      if (now - start > TLB_PANIC_SECS * hz)
        panic("cpu%d does not answer a TLB flush", i);
      if (now > resend) {
        tlb_send(ci);
        resend = now + TLB_RESEND_SECS * hz;
      }
    }
  }
}

/* Every processor that may hold translations of mm has dropped them all.  For
   a mapping removed or made less permissive; the caller frees the pages it
   freed from mm only after this. */
void tlb_shootdown(struct mm *mm) {
  struct cpu_info *me = curcpu();
  uint64_t f = irq_save();
  uint64_t self = 1UL << me->ci_index, mask = mm->cpus;
  if (tlb_diag) {
    mask = 0;
    for (int i = 0; i < MAXCPUS; i++)
      if (cpu_info_list[i] && cpu_info_list[i]->ci_state == CPU_RUNNING)
        mask |= 1UL << i;
  }
  if ((mask & self) || read_cr3() == mm->pml4)
    write_cr3(read_cr3());
  mask &= ~self;
  if (mask)
    tlb_remote(mask);
  irq_restore(f);
}

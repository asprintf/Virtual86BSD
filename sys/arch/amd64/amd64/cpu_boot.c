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

/* cpu_boot.c - starting the application processors.  The boot processor
   prepares a processor's data, its idle task and stack, then sends it INIT
   and two start-up IPIs (Intel SDM vol. 3, 8.4) to the code of mptramp.s,
   one processor at a time, and waits for it to say it is running.  A started
   processor parks in a halt loop; no task is scheduled to it yet. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>
#include <machine/i82489reg.h>

#define START_TIMEOUT_MS 1000

extern char ap_tramp_start[], ap_tramp_end[], ap_tramp_gdt[], ap_tramp_gdtptr[], ap_pml4[], boot_pdpt_low[];
extern uint64_t ap_boot_rsp, ap_boot_ci, ap_cr0, ap_cr4;

static void udelay(uint64_t us) {
  uint64_t end = rdtsc64() + us * (tsc_frequency() / 1000000);
  while (rdtsc64() < end)
    cpu_pause();
}

/* what this processor is: the brand string and the family-model-stepping */
void cpu_identify(struct cpu_info *ci) {
  uint32_t r[4], b[12];
  cpuid4(1, 0, r);
  ci->ci_signature = r[0];
  ci->ci_apicid_seen = r[1] >> 24;
  ci->ci_brand[0] = 0;
  cpuid4(0x80000000U, 0, r);
  if (r[0] >= 0x80000004U) {
    for (int i = 0; i < 3; i++)
      cpuid4(0x80000002U + i, 0, b + 4 * i);
    memcpy(ci->ci_brand, b, 48);
    ci->ci_brand[48] = 0;
  }
}

/* the data of processor number index (not the boot processor); NULL if out of memory */
struct cpu_info *cpu_alloc(int index, uint32_t apicid) {
  struct cpu_info *ci = kzalloc(sizeof(*ci));
  uint64_t df = alloc_pages(1);
  struct task *idle = idle_task_alloc();
  if (!ci || !df || !idle) {
    if (df)
      free_pages(df, 1);
    if (idle)
      task_free(idle);
    kfree(ci);
    return NULL;
  }
  ci->ci_index = index;
  ci->ci_apicid = apicid;
  ci->ci_self = ci;
  ci->ci_idle = idle;
  ci->ci_curtask = idle;
  idle->oncpu = index;
  cpu_tss_init(ci, (uint64_t)P2V(df) + 2 * PAGE_SIZE);
  cpu_info_list[index] = ci;
  return ci;
}

/* A processor that is running: it takes the page tables of the kernel,
   loads its own tables and enables its local APIC, then says so and parks. */
void ap_main(struct cpu_info *ci) {
  write_cr3(kernel_pml4);
  cpu_tables_load(ci);
  cpu_identify(ci);
  lapic_init_ap();
  mb();
  ci->ci_state = CPU_RUNNING;
  for (;;) {
    cli();
    hlt();
  }
}

/* Start processor ci: 0 when it is running (the boot processor is, and
   needs no IPI), else -1 and the reason said.  A processor is started once. */
int cpu_boot(struct cpu_info *ci) {
  if (ci->ci_state != CPU_NOTSTARTED)
    panic("cpu%d already started", ci->ci_index);
  if (ci == curcpu()) {
    ci->ci_state = CPU_RUNNING;
    return 0;
  }
  ci->ci_state = CPU_STARTING;
  uint64_t page = lowmem_page();
  if (!lapic_enabled || !page) {
    printf("cpu%d: %s\n", ci->ci_index, lapic_enabled ? "no memory below 1 MiB to start it from" : "no local APIC");
    ci->ci_state = CPU_FAILED;
    return -1;
  }

  /* the page tables it starts with: the kernel's, and the first GiB where it is (it runs the start-up code there) */
  uint64_t *pml4 = (uint64_t *)ap_pml4;
  memcpy(pml4, P2V(kernel_pml4), PAGE_SIZE);
  pml4[0] = ((uint64_t)boot_pdpt_low - KERNEL_BASE) | PTE_P | PTE_W;
  ap_cr0 = read_cr0();
  ap_cr4 = read_cr4();
  ap_boot_rsp = task_kstack_top(ci->ci_idle);
  ap_boot_ci = (uint64_t)ci;

  char *code = P2V(page);
  memcpy(code, ap_tramp_start, (size_t)(ap_tramp_end - ap_tramp_start));
  uint32_t gdt_pa = (uint32_t)page + (uint32_t)(ap_tramp_gdt - ap_tramp_start);
  memcpy(code + (ap_tramp_gdtptr - ap_tramp_start) + 2, &gdt_pa, sizeof gdt_pa);
  mb();

  uint32_t sipi = LAPIC_DM_STARTUP | LAPIC_ICR_ASSERT | (uint32_t)(page >> 12);
  bool sent = lapic_ipi(ci->ci_apicid, LAPIC_DM_INIT | LAPIC_ICR_ASSERT);
  udelay(10000);
  sent = sent && lapic_ipi(ci->ci_apicid, sipi);
  udelay(200);
  if (sent && ci->ci_state == CPU_STARTING) {
    sent = lapic_ipi(ci->ci_apicid, sipi);
    udelay(200);
  }
  uint64_t end = rdtsc64() + START_TIMEOUT_MS * (tsc_frequency() / 1000);
  while (sent && ci->ci_state == CPU_STARTING && rdtsc64() < end)
    cpu_pause();
  if (ci->ci_state == CPU_RUNNING)
    return 0;

  /* it did not answer: INIT puts it back to waiting for a start-up IPI; its stack is not used again */
  lapic_ipi(ci->ci_apicid, LAPIC_DM_INIT | LAPIC_ICR_ASSERT);
  ci->ci_state = CPU_FAILED;
  printf("cpu%d: failed to start\n", ci->ci_index);
  return -1;
}

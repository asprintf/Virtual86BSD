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

/* lapic.c - the local APIC: the boot processor's enabled beside the 8259
   (which keeps delivering the devices' interrupts through LINT0), its timer
   measured, the inter-processor interrupts, and the others' set-up. */
#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>
#include <machine/i82489reg.h>

bool lapic_enabled;
uint64_t lapic_hz; /* the timer's count rate (divided by 1); 0 if it could not be measured */
static volatile uint32_t *lapic;
static void (*ipi_fn[IPI_VEC_LAST - IPI_VEC_FIRST + 1])(void);

static uint32_t lapic_read(int reg) { return lapic[reg >> 2]; }
static void lapic_write(int reg, uint32_t v) { lapic[reg >> 2] = v; }

void lapic_eoi(void) { lapic_write(LAPIC_EOI, 0); }

/* fixed-delivery IPI to this processor */
void lapic_ipi_self(int vec) {
  lapic_write(LAPIC_ICR_LO, LAPIC_ICR_SELF | LAPIC_ICR_ASSERT | (uint32_t)vec);
}

void ipi_register(int vec, void (*fn)(void)) {
  if (vec < IPI_VEC_FIRST || vec > IPI_VEC_LAST)
    panic("ipi_register: vector %d", vec);
  ipi_fn[vec - IPI_VEC_FIRST] = fn;
}

/* make ci take the IPI_VEC_RESCHED interrupt (the caller holds the kernel lock) */
void ipi_resched(struct cpu_info *ci) {
  lapic_ipi(ci->ci_apicid, LAPIC_ICR_ASSERT | IPI_VEC_RESCHED);
}

/* an IPI vector arrived (trap_handler) */
void lapic_intr(int vec) {
  lapic_eoi();
  if (ipi_fn[vec - IPI_VEC_FIRST])
    ipi_fn[vec - IPI_VEC_FIRST]();
}

/* the timer's rate, by the TSC (which time_init measured) over 10 ms */
static uint64_t lapic_calibrate(void) {
  lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV_1);
  lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED); /* one-shot, no interrupt */
  lapic_write(LAPIC_TIMER_ICR, 0xffffffffU);
  uint64_t t0 = rdtsc64(), t1;
  do {
    cpu_pause();
    t1 = rdtsc64();
  } while (t1 - t0 < tsc_frequency() / 100);
  uint32_t left = lapic_read(LAPIC_TIMER_CCR);
  lapic_write(LAPIC_TIMER_ICR, 0);
  if (left == 0 || left == 0xffffffffU)
    return 0;
  return (uint64_t)(0xffffffffU - left) * tsc_frequency() / (t1 - t0);
}

/* the registers every local APIC is given: the 8259's interrupts come to the
   boot processor's LINT0 only */
static void lapic_setup(bool boot) {
  lapic_write(LAPIC_TPR, 0);
  lapic_write(LAPIC_LVT_LINT0, boot ? LAPIC_DM_EXTINT : LAPIC_LVT_MASKED | LAPIC_DM_EXTINT);
  lapic_write(LAPIC_LVT_LINT1, LAPIC_DM_NMI);
  lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
  lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);
  lapic_write(LAPIC_ESR, 0);
  lapic_write(LAPIC_ESR, 0);
  lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xff);
}

/* an application processor's own local APIC, at the address the boot processor mapped */
void lapic_init_ap(void) { lapic_setup(false); }

/* this processor's timer: HZ interrupts a second on IPI_VEC_TIMER, at the rate the boot processor measured */
void lapic_timer_start(void) {
  lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV_1);
  lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_PERIODIC | IPI_VEC_TIMER);
  lapic_write(LAPIC_TIMER_ICR, (uint32_t)(lapic_hz / HZ));
}

/* wait for the APIC to have sent the last IPI (100 ms at most) */
static bool lapic_icr_idle(void) {
  uint64_t end = rdtsc64() + tsc_frequency() / 10;
  while (lapic_read(LAPIC_ICR_LO) & LAPIC_ICR_PENDING) {
    if (rdtsc64() > end)
      return false;
    cpu_pause();
  }
  return true;
}

/* an IPI to the processor with this APIC id; cmd is the ICR's low word (delivery
   mode, vector ...); false if the APIC did not send it */
bool lapic_ipi(uint32_t apicid, uint32_t cmd) {
  uint64_t f = irq_save();
  bool ok = lapic_icr_idle();
  if (ok) {
    lapic_write(LAPIC_ICR_HI, apicid << 24);
    lapic_write(LAPIC_ICR_LO, cmd);
    ok = lapic_icr_idle();
  }
  irq_restore(f);
  return ok;
}

/* Switch the local APIC on, when it is there and in the mode we use; when it
   is not, the processor stays as it was (8259 only). */
void lapic_init(const char *name) {
  uint32_t r[4];
  if (!lapic_pa) /* no ACPI: no list of processors, nothing to do with the APIC */
    return;
  cpuid4(1, 0, r);
  if (!(r[3] & (1U << 9))) {
    printf("%s: no local APIC\n", name);
    return;
  }
  uint64_t base = rdmsr(MSR_APIC_BASE);
  if (!(base & APIC_BASE_ENABLE)) {
    printf("%s: local APIC disabled\n", name);
    return;
  }
  if (base & APIC_BASE_X2APIC) {
    printf("%s: local APIC in x2APIC mode, not used\n", name);
    return;
  }
  lapic = ioremap(base & APIC_BASE_ADDR, PAGE_SIZE);
  if (!lapic) {
    printf("%s: cannot map the local APIC\n", name);
    return;
  }
  lapic_setup(true);
  lapic_enabled = true;

  lapic_hz = lapic_calibrate();
  if (lapic_hz < 1000000UL || lapic_hz > 10000000000UL) {
    printf("%s: apic clock not usable\n", name);
    lapic_hz = 0;
  } else
    printf("%s: apic clock running at %luMHz\n", name, (lapic_hz + 500000) / 1000000);
}

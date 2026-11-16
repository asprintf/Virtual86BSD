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

/* i82489reg.h - the local APIC (xAPIC): its registers, memory-mapped as 32 bits
   at 16-byte steps, and the bits we use. */
#ifndef _MACHINE_I82489REG_H_
#define _MACHINE_I82489REG_H_

#define LAPIC_ID        0x020
#define LAPIC_VERS      0x030
#define LAPIC_TPR       0x080
#define LAPIC_EOI       0x0b0
#define LAPIC_SVR       0x0f0
#define LAPIC_ESR       0x280
#define LAPIC_ICR_LO    0x300
#define LAPIC_ICR_HI    0x310
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_TIMER_ICR 0x380
#define LAPIC_TIMER_CCR 0x390
#define LAPIC_TIMER_DCR 0x3e0

#define LAPIC_SVR_ENABLE 0x100    /* software enable; the low byte is the spurious vector */
#define LAPIC_LVT_MASKED 0x10000
#define LAPIC_LVT_PERIODIC 0x20000 /* timer mode (LVT bit 17): periodic */
#define LAPIC_DM_NMI     0x400    /* delivery modes (LVT bits 10:8, ICR bits 10:8) */
#define LAPIC_DM_INIT    0x500
#define LAPIC_DM_STARTUP 0x600
#define LAPIC_DM_EXTINT  0x700    /* an interrupt of the 8259 (virtual wire) */
#define LAPIC_ICR_PENDING 0x1000  /* delivery status: the last IPI is not yet sent */
#define LAPIC_ICR_ASSERT  0x4000
#define LAPIC_ICR_SELF    0x40000 /* destination shorthand: self */
#define LAPIC_TIMER_DIV_1 0xb     /* divide configuration: by 1 */

#define MSR_APIC_BASE      0x1b
#define APIC_BASE_X2APIC   (1UL << 10)
#define APIC_BASE_ENABLE   (1UL << 11)
#define APIC_BASE_ADDR     0x000ffffffffff000UL

#endif

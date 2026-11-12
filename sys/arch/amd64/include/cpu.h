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

/* arch.h - x86-64 specifics: register frame, CPU helpers (arch/cpu.s). */
#ifndef ARCH_H
#define ARCH_H

#include <sys/systm.h>

struct regs {
  uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8;
  uint64_t rax, rcx, rdx, rsi, rdi;
  uint64_t vector, err; /* err = orig_rax for a system call */
  uint64_t rip, cs, rflags, rsp, ss;
};

#define VEC_SYSCALL 0x100

/* segment selectors, as Linux */
#define KERNEL_CS 0x10
#define KERNEL_DS 0x18
#define USER32_CS 0x23
#define USER_DS 0x2b
#define USER_CS 0x33
#define TSS_SEL 0x40

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084
#define MSR_FS_BASE 0xC0000100
#define MSR_GS_BASE 0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

void outb(uint16_t port, uint8_t v);
void outw(uint16_t port, uint16_t v);
void outl(uint16_t port, uint32_t v);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
void insw(uint16_t port, void *buf, size_t count);
void outsw(uint16_t port, const void *buf, size_t count);
uint64_t read_cr0(void), read_cr2(void), read_cr3(void), read_cr4(void);
void write_cr0(uint64_t), write_cr3(uint64_t), write_cr4(uint64_t);
void invlpg(uint64_t va);
uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t v);
uint64_t rdtsc64(void);
void mb(void);        /* mfence */
void cpu_pause(void); /* pause */
void lgdt_reload(void *ptr);
void lidt(void *ptr);
void ltr(uint16_t sel);
void cli(void), sti(void), hlt(void), sti_hlt(void);
uint64_t read_rflags(void);
uint64_t irq_save(void);
void irq_restore(uint64_t flags);
void fxsave(void *area), fxrstor(void *area), fninit_sse(void);
void cpuid4(uint32_t leaf, uint32_t sub, uint32_t out[4]);
int rdrand64(uint64_t *out, int seed); /* 1: out is the processor's random number (seed: RDSEED, else RDRAND) */

void switch_stack(uint64_t *save_sp, uint64_t new_sp);
extern uint64_t kstack_top;
void ret_from_fork(void);
void kthread_start(void);
void trap_return(void);
void syscall_entry(void);

long user_copy(void *dst, const void *src, size_t n);
long user_strnlen(const char *s, size_t max);
long user_set(void *dst, int c, size_t n);
extern char user_access_begin[], user_access_end[], user_access_fault[];

/* arch/x86.c */
void arch_init(void);
void set_kernel_stack(uint64_t top);
void irq_register(int irq, void (*fn)(void));
void pic_eoi(int irq);

#endif

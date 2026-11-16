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

/* x86.c - descriptor tables, interrupt controller, timer, system call MSRs,
   and the trap dispatcher. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

/* GDT: the layout of Linux (selectors in arch.h), a TSS for each processor from 0x40 */
static uint64_t gdt[8 + 2 * MAXCPUS] = {
    0,
    0x00cf9a000000ffff, /* 0x08 kernel 32-bit code (unused) */
    0x00af9a000000ffff, /* 0x10 kernel code */
    0x00cf92000000ffff, /* 0x18 kernel data */
    0x00cffa000000ffff, /* 0x23 user 32-bit code */
    0x00cff2000000ffff, /* 0x2b user data */
    0x00affa000000ffff, /* 0x33 user code */
    0,
};

/* the 64-bit TSS of a processor (104 bytes) is ci_tss, as 32-bit words */
#define TSS_RSP0 1
#define TSS_IST1 9
static void tss_set64(struct cpu_info *ci, int word, uint64_t v) {
  ci->ci_tss[word] = (uint32_t)v;
  ci->ci_tss[word + 1] = (uint32_t)(v >> 32);
}
static uint64_t df_stack[1024];
struct cpu_info cpu_info_primary;
struct cpu_info *cpu_info_list[MAXCPUS] = {&cpu_info_primary};

struct idt_entry {
  uint16_t off0, sel;
  uint8_t ist, type;
  uint16_t off1;
  uint32_t off2, zero;
};
static struct idt_entry idt[256];

/* a descriptor-table pointer: 16-bit limit, then the 64-bit base */
static void set_dtr(uint16_t d[5], size_t limit, uint64_t base) {
  d[0] = (uint16_t)limit;
  for (int i = 0; i < 4; i++)
    d[1 + i] = (uint16_t)(base >> 16 * i);
}

extern uint64_t isr_table[48], isr_ipi_table[IPI_VEC_LAST - IPI_VEC_FIRST + 1];
void isr255(void);

static void set_gate(int n, uint64_t fn, int dpl, int ist) {
  idt[n].off0 = (uint16_t)fn;
  idt[n].sel = KERNEL_CS;
  idt[n].ist = (uint8_t)ist;
  idt[n].type = (uint8_t)(0x8e | dpl << 5);
  idt[n].off1 = (uint16_t)(fn >> 16);
  idt[n].off2 = (uint32_t)(fn >> 32);
  idt[n].zero = 0;
}

void set_kernel_stack(uint64_t top) {
  struct cpu_info *ci = curcpu();
  tss_set64(ci, TSS_RSP0, top);
  ci->ci_kstack_top = top;
}

/* 8259 PICs, remapped to vectors 32..47 */
#define IRQ_SHARE 4 /* handlers on one line: PCI cards share (level-triggered) lines */
static void (*irq_fn[16][IRQ_SHARE])(void);

static void pic_init(void) {
  outb(0x20, 0x11);
  outb(0xa0, 0x11);
  outb(0x21, 32);
  outb(0xa1, 40);
  outb(0x21, 4);
  outb(0xa1, 2);
  outb(0x21, 1);
  outb(0xa1, 1);
  outb(0x21, 0xfb); /* all masked but the cascade */
  outb(0xa1, 0xff);
}

static void irq_unmask(int irq) {
  if (irq < 8)
    outb(0x21, inb(0x21) & ~(1 << irq));
  else
    outb(0xa1, inb(0xa1) & ~(1 << (irq - 8)));
}

/* fn is called on every interrupt of the line, with the others sharing it:
   it looks at its own device and does nothing when that was not the one */
void irq_register(int irq, void (*fn)(void)) {
  int i = 0;
  while (i < IRQ_SHARE && irq_fn[irq][i] && irq_fn[irq][i] != fn)
    i++;
  if (i == IRQ_SHARE)
    panic("irq_register: too many devices on irq %d", irq);
  irq_fn[irq][i] = fn;
  irq_unmask(irq);
}

void pic_eoi(int irq) {
  if (irq >= 8)
    outb(0xa0, 0x20);
  outb(0x20, 0x20);
}

/* the TSS of ci, with df_top the top of its stack for double faults, in the GDT */
void cpu_tss_init(struct cpu_info *ci, uint64_t df_top) {
  uint64_t t = (uint64_t)ci->ci_tss;
  int i = 8 + 2 * ci->ci_index;
  gdt[i] = (sizeof(ci->ci_tss) - 1) | (t & 0xffffff) << 16 | 0x89UL << 40 | ((t >> 24) & 0xff) << 56;
  gdt[i + 1] = t >> 32;
  ci->ci_tss[25] = (uint32_t)sizeof(ci->ci_tss) << 16; /* no I/O permission map */
  tss_set64(ci, TSS_IST1, df_top);
}

/* this processor's tables, %gs and system call entry (ci is its own) */
void cpu_tables_load(struct cpu_info *ci) {
  uint16_t g[5], d[5];
  set_dtr(g, sizeof(gdt) - 1, (uint64_t)gdt);
  lgdt_reload(g);
  gs_init(ci); /* (the reload cleared the base) */
  ltr(TSS_SEL(ci->ci_index));
  set_dtr(d, sizeof(idt) - 1, (uint64_t)idt);
  lidt(d);

  /* system calls */
  wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1); /* SCE */
  wrmsr(MSR_STAR, (uint64_t)KERNEL_CS << 32 | (uint64_t)(USER32_CS - 3) << 48);
  wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
  wrmsr(MSR_SFMASK, 0x47700); /* IF, TF, DF, AC, NT, IOPL cleared on entry */
}

void arch_init(void) {
  struct cpu_info *ci = &cpu_info_primary;
  ci->ci_self = ci;
  cpu_tss_init(ci, (uint64_t)df_stack + sizeof(df_stack));
  for (int i = 0; i < 48; i++)
    set_gate(i, isr_table[i], 0, i == 8 ? 1 : 0);
  set_gate(3, isr_table[3], 3, 0); /* int3 from user mode */
  for (int i = IPI_VEC_FIRST; i <= IPI_VEC_LAST; i++)
    set_gate(i, isr_ipi_table[i - IPI_VEC_FIRST], 0, 0);
  set_gate(255, (uint64_t)isr255, 0, 0);
  cpu_tables_load(ci);
  bkl_armed = true;
  pic_init();

  /* SSE for user programs: CR0.MP, !EM; CR4.OSFXSR, OSXMMEXCPT */
  write_cr0((read_cr0() & ~4UL) | 2);
  write_cr4(read_cr4() | 0x600);
}

static void dump_regs(struct regs *r) {
  printf("rip %016lx cs %lx rflags %lx rsp %016lx ss %lx\n", r->rip, r->cs, r->rflags, r->rsp, r->ss);
  printf("rax %016lx rbx %016lx rcx %016lx rdx %016lx\n", r->rax, r->rbx, r->rcx, r->rdx);
  printf("rsi %016lx rdi %016lx rbp %016lx r8  %016lx\n", r->rsi, r->rdi, r->rbp, r->r8);
  printf("r9  %016lx r10 %016lx r11 %016lx r12 %016lx\n", r->r9, r->r10, r->r11, r->r12);
  printf("r13 %016lx r14 %016lx r15 %016lx cr2 %016lx\n", r->r13, r->r14, r->r15, read_cr2());
}

static const char *exc_name[32] = {
    "divide error", "debug", "NMI", "breakpoint", "overflow", "bound range",
    "invalid opcode", "device not available", "double fault", "coprocessor overrun",
    "invalid TSS", "segment not present", "stack fault", "general protection",
    "page fault", "15", "x87 error", "alignment check", "machine check", "SIMD error",
    "virtualization", "control protection"};

static void trap_dispatch(struct regs *r) {
  uint64_t v = r->vector;
  if (v == 14) {
    page_fault(r);
    return;
  }
  if (v >= 32 && v < 48) {
    int irq = (int)v - 32;
    struct cpu_info *ci = curcpu();
    ci->ci_irq_nesting++;
    pic_eoi(irq);
    if (irq == 0)
      timer_tick((r->cs & 3) == 3);
    for (int i = 0; i < IRQ_SHARE && irq_fn[irq][i]; i++)
      irq_fn[irq][i]();
    ci->ci_irq_nesting--;
    return;
  }
  if (v >= IPI_VEC_FIRST && v <= IPI_VEC_LAST) {
    struct cpu_info *ci = curcpu();
    ci->ci_irq_nesting++;
    if (v == IPI_VEC_TIMER) {
      lapic_eoi();
      cpu_tick((r->cs & 3) == 3);
    } else
      lapic_intr((int)v);
    ci->ci_irq_nesting--;
    return;
  }
  if (v == 255)
    return;
  if ((r->cs & 3) == 3) {
    /* an exception in a user program: a signal, as Linux */
    int sig = SIGSEGV, code = SI_KERNEL;
    switch (v) {
    case 0: sig = SIGFPE; code = FPE_INTDIV; break;
    case 1: case 3: sig = SIGTRAP; code = SI_KERNEL; break;
    case 4: case 5: sig = SIGSEGV; break;
    case 6: sig = SIGILL; code = ILL_ILLOPN; break;
    case 16: sig = SIGFPE; code = 0; break;
    case 19: sig = SIGFPE; code = FPE_FLTINV; break;
    case 17: sig = SIGBUS; code = BUS_ADRALN; break;
    }
    force_signal(sig, code, v == 0 || v == 6 || v == 16 || v == 19 ? r->rip : 0, (int)v, r->err);
    return;
  }
  printf("\nkernel: %s trap, code=%lx\n", v < 32 ? exc_name[v] : "unknown", r->err);
  dump_regs(r);
  panic("trap type %lu in kernel mode", v);
}

/* Every entry from user mode takes the kernel lock (trap_return gives it back).
   One from kernel mode already has it, except in the idle halt; a fatal
   exception in the kernel may come anywhere, and takes none. */
void trap_handler(struct regs *r) {
  uint64_t v = r->vector;
  bool user = (r->cs & 3) == 3, took = false;
  struct cpu_info *ci = curcpu();
  if (user)
    bkl_lock();
  else if (v == 2 || v == 8 || v == 18)
    ci->ci_nolock = true;
  else if (!bkl_held()) {
    bkl_lock();
    took = true;
  }
  trap_dispatch(r);
  if (took)
    bkl_unlock();
}

void kernel_fault(struct regs *r, uint64_t addr) {
  printf("\nkernel: page fault trap, code=%lx\nfault address %016lx\n", r->err, addr);
  dump_regs(r);
  panic("trap type 14 in kernel mode");
}

/* the clock: the PIT at HZ */
void cpu_initclocks(void) {
  unsigned div = 1193182 / HZ;
  outb(0x43, 0x34);
  outb(0x40, div & 0xff);
  outb(0x40, div >> 8);
  irq_unmask(0);
}

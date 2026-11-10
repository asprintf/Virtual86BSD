/* x86.c - descriptor tables, interrupt controller, timer, system call MSRs,
   and the trap dispatcher. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

void account_tick(bool user);

/* GDT: the layout of Linux (selectors in arch.h), TSS at 0x40 */
static uint64_t gdt[10] = {
    0,
    0x00cf9a000000ffff, /* 0x08 kernel 32-bit code (unused) */
    0x00af9a000000ffff, /* 0x10 kernel code */
    0x00cf92000000ffff, /* 0x18 kernel data */
    0x00cffa000000ffff, /* 0x23 user 32-bit code */
    0x00cff2000000ffff, /* 0x2b user data */
    0x00affa000000ffff, /* 0x33 user code */
    0,
    0, 0,               /* 0x40 TSS (16 bytes) */
};

/* the 64-bit TSS (104 bytes, 4-byte aligned fields): kept as 32-bit words
   because the 64-bit fields are unaligned */
static uint32_t tss[26];
#define TSS_RSP0 1
#define TSS_IST1 9
static void tss_set64(int word, uint64_t v) {
  tss[word] = (uint32_t)v;
  tss[word + 1] = (uint32_t)(v >> 32);
}
static uint64_t df_stack[1024];

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

extern uint64_t isr_table[48];
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
  tss_set64(TSS_RSP0, top);
  kstack_top = top;
}

/* 8259 PICs, remapped to vectors 32..47 */
static void (*irq_fn[16])(void);

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

void irq_register(int irq, void (*fn)(void)) {
  irq_fn[irq] = fn;
  if (irq < 8)
    outb(0x21, inb(0x21) & ~(1 << irq));
  else
    outb(0xa1, inb(0xa1) & ~(1 << (irq - 8)));
}

void pic_eoi(int irq) {
  if (irq >= 8)
    outb(0xa0, 0x20);
  outb(0x20, 0x20);
}

void arch_init(void) {
  uint64_t t = (uint64_t)tss;
  gdt[8] = (sizeof(tss) - 1) | (t & 0xffffff) << 16 | 0x89UL << 40 | ((t >> 24) & 0xff) << 56;
  gdt[9] = t >> 32;
  tss[25] = (uint32_t)sizeof(tss) << 16; /* no I/O permission map */
  tss_set64(TSS_IST1, (uint64_t)df_stack + sizeof(df_stack));
  uint16_t g[5];
  set_dtr(g, sizeof(gdt) - 1, (uint64_t)gdt);
  lgdt_reload(g);
  ltr(TSS_SEL);

  for (int i = 0; i < 48; i++)
    set_gate(i, isr_table[i], 0, i == 8 ? 1 : 0);
  set_gate(3, isr_table[3], 3, 0); /* int3 from user mode */
  set_gate(255, (uint64_t)isr255, 0, 0);
  uint16_t d[5];
  set_dtr(d, sizeof(idt) - 1, (uint64_t)idt);
  lidt(d);
  pic_init();

  /* SSE for user programs: CR0.MP, !EM; CR4.OSFXSR, OSXMMEXCPT */
  write_cr0((read_cr0() & ~4UL) | 2);
  write_cr4(read_cr4() | 0x600);

  /* system calls */
  wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1); /* SCE */
  wrmsr(MSR_STAR, (uint64_t)KERNEL_CS << 32 | (uint64_t)(USER32_CS - 3) << 48);
  wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
  wrmsr(MSR_SFMASK, 0x47700); /* IF, TF, DF, AC, NT, IOPL cleared on entry */
}

static void dump_regs(struct regs *r) {
  printk("rip %016lx cs %lx rflags %lx rsp %016lx ss %lx\n", r->rip, r->cs, r->rflags, r->rsp, r->ss);
  printk("rax %016lx rbx %016lx rcx %016lx rdx %016lx\n", r->rax, r->rbx, r->rcx, r->rdx);
  printk("rsi %016lx rdi %016lx rbp %016lx r8  %016lx\n", r->rsi, r->rdi, r->rbp, r->r8);
  printk("r9  %016lx r10 %016lx r11 %016lx r12 %016lx\n", r->r9, r->r10, r->r11, r->r12);
  printk("r13 %016lx r14 %016lx r15 %016lx cr2 %016lx\n", r->r13, r->r14, r->r15, read_cr2());
}

static const char *exc_name[32] = {
    "divide error", "debug", "NMI", "breakpoint", "overflow", "bound range",
    "invalid opcode", "device not available", "double fault", "coprocessor overrun",
    "invalid TSS", "segment not present", "stack fault", "general protection",
    "page fault", "15", "x87 error", "alignment check", "machine check", "SIMD error",
    "virtualization", "control protection"};

void trap_handler(struct regs *r) {
  uint64_t v = r->vector;
  if (v == 14) {
    page_fault(r);
    return;
  }
  if (v >= 32 && v < 48) {
    int irq = (int)v - 32;
    extern int irq_nesting;
    irq_nesting++;
    pic_eoi(irq);
    if (irq == 0)
      account_tick((r->cs & 3) == 3);
    if (irq_fn[irq])
      irq_fn[irq]();
    irq_nesting--;
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
  printk("\n%s (vector %lu, error %lx) in kernel mode\n", v < 32 ? exc_name[v] : "?", v, r->err);
  dump_regs(r);
  panic("unexpected exception");
}

void kernel_fault(struct regs *r, uint64_t addr) {
  printk("\nkernel page fault at %016lx (error %lx)\n", addr, r->err);
  dump_regs(r);
  panic("page fault in kernel mode");
}

/* PIT at HZ */
void pit_init(void (*tick)(void)) {
  unsigned div = 1193182 / HZ;
  outb(0x43, 0x34);
  outb(0x40, div & 0xff);
  outb(0x40, div >> 8);
  irq_register(0, tick);
}

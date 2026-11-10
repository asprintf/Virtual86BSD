/* syscall.c - the system call entry: entry.s calls syscall_dispatch with the
   registers of the caller, and the table of the process's emulation
   (struct emul) says what the call number means. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/cpu.h>

void syscall_dispatch(struct regs *r) {
  const struct emul *e = current->proc->p_emul;
  uint64_t nr = r->rax;
  current->regs = r;
  long ret = -ENOSYS;
  if (nr < (uint64_t)e->e_nsysent)
    ret = e->e_sysent[nr].sy_call((long)r->rdi, (long)r->rsi, (long)r->rdx, (long)r->r10,
                                  (long)r->r8, (long)r->r9);
  if (nr == (uint64_t)e->e_sigreturn) /* the restored rax stands */
    return;
  r->rax = (uint64_t)ret;
}

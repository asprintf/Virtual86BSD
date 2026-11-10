/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

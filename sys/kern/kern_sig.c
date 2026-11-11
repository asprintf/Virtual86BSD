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

/* signal.c - generation, delivery (Linux x86-64 rt_sigframe), job control
   stops, and the signal system calls. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define STOP_SIGS (SIGBIT(SIGSTOP) | SIGBIT(SIGTSTP) | SIGBIT(SIGTTIN) | SIGBIT(SIGTTOU))
#define IGNORE_SIGS (SIGBIT(SIGCHLD) | SIGBIT(SIGCONT) | SIGBIT(SIGURG) | SIGBIT(SIGWINCH))
#define CORE_SIGS (SIGBIT(SIGQUIT) | SIGBIT(SIGILL) | SIGBIT(SIGTRAP) | SIGBIT(SIGABRT) | \
                   SIGBIT(SIGBUS) | SIGBIT(SIGFPE) | SIGBIT(SIGSEGV) | SIGBIT(SIGXCPU) |   \
                   SIGBIT(SIGXFSZ) | SIGBIT(SIGSYS))

static bool default_ignored(int sig) { return sig < SIGRTMIN && (IGNORE_SIGS & SIGBIT(sig)); }

bool sig_ignored(struct proc *p, int sig) {
  uint64_t h = p->sigact[sig - 1].handler;
  return h == SIG_IGN || (h == SIG_DFL && default_ignored(sig));
}

bool signal_pending(struct task *t) {
  if (!t->proc)
    return false;
  if (t->proc->group_exit || (t->proc->stopped && !t->exiting))
    return true;
  return ((t->pending.set | t->proc->pending.set) & ~t->blocked) != 0;
}

static void signal_wake(struct task *t, bool kill) {
  if (t->state == T_SLEEPING || (kill && (t->state == T_UNINTERRUPTIBLE || t->state == T_STOPPED)))
    wake_task(t);
  if (kill && t->state == T_STOPPED) {
    t->state = T_RUNNING;
    make_runnable(t);
  }
}

static void sp_remove(struct proc *pr, struct sigpending *sp, sigset_t mask) {
  sp->set &= ~mask;
  list_for_each_safe(p, n, &sp->queue) {
    struct sigqueue *q = list_entry(p, struct sigqueue, link);
    if (mask & SIGBIT(q->info.signo)) {
      if (q->info.code == SI_TIMER)
        timer_signal_taken(pr, &q->info);
      list_del(p);
      kfree(q);
    }
  }
}

static void proc_continue(struct proc *p, bool notify) {
  if (!p->stopped)
    return;
  p->stopped = false;
  p->report = CLD_CONTINUED;
  list_for_each(q, &p->threads) {
    struct task *t = list_entry(q, struct task, thread_link);
    if (t->state == T_STOPPED) {
      t->state = T_RUNNING;
      make_runnable(t);
    }
  }
  struct proc *pp = p->parent;
  if (notify && pp != p) {
    if (!(pp->sigact[SIGCHLD - 1].flags & SA_NOCLDSTOP)) {
      struct siginfo si;
      memset(&si, 0, sizeof(si));
      si.signo = SIGCHLD;
      si.code = CLD_CONTINUED;
      si.u.chld.pid = p->pid;
      si.u.chld.uid = p->cred.uid;
      si.u.chld.status = SIGCONT;
      send_signal_proc(pp, &si);
    }
    wake_up(&pp->child_wait);
  }
}

/* whether the last send_signal_* queued its siginfo (a POSIX timer's
 * signal that was not queued is an overrun of the timer) */
static bool queued_last;
bool signal_was_queued(void) { return queued_last; }

static int queue_signal(struct sigpending *sp, const struct siginfo *si) {
  int sig = si->signo;
  if (sig < SIGRTMIN && (sp->set & SIGBIT(sig)))
    return 0; /* already pending */
  struct sigqueue *q = kmalloc(sizeof(*q));
  if (!q) {
    if (sig >= SIGRTMIN)
      return -EAGAIN;
  } else {
    q->info = *si;
    list_add_tail(&sp->queue, &q->link);
    queued_last = true;
  }
  sp->set |= SIGBIT(sig);
  return 0;
}

/* common part: true if the signal should be queued at all */
static bool prepare_signal(struct proc *p, int sig, struct task *t) {
  if (p->state != P_ALIVE)
    return false;
  if (sig == SIGCONT || sig == SIGKILL) {
    sp_remove(p, &p->pending, STOP_SIGS);
    list_for_each(q, &p->threads) sp_remove(p, &list_entry(q, struct task, thread_link)->pending, STOP_SIGS);
    proc_continue(p, true);
  } else if (SIGBIT(sig) & STOP_SIGS) {
    sp_remove(p, &p->pending, SIGBIT(SIGCONT));
    list_for_each(q, &p->threads) sp_remove(p, &list_entry(q, struct task, thread_link)->pending, SIGBIT(SIGCONT));
  }
  if (sig == SIGKILL)
    return true;
  /* ignored and not blocked: discarded */
  struct task *any = t ? t : (list_empty(&p->threads) ? NULL : list_entry(p->threads.next, struct task, thread_link));
  if (sig_ignored(p, sig) && !(any && (any->blocked & SIGBIT(sig))))
    return false;
  return true;
}

int send_signal_proc(struct proc *p, const struct siginfo *si) {
  int sig = si->signo;
  queued_last = false;
  if (sig == 0)
    return 0;
  uint64_t f = irq_save();
  if (!prepare_signal(p, sig, NULL)) {
    irq_restore(f);
    return 0;
  }
  int r = queue_signal(&p->pending, si);
  /* wake a thread that will take it */
  bool kill = sig == SIGKILL;
  list_for_each(q, &p->threads) {
    struct task *t = list_entry(q, struct task, thread_link);
    if (kill) {
      t->pending.set |= SIGBIT(SIGKILL);
      signal_wake(t, true);
    } else if (!(t->blocked & SIGBIT(sig))) {
      signal_wake(t, false);
      break;
    }
  }
  irq_restore(f);
  return r;
}

int send_signal_task(struct task *t, const struct siginfo *si) {
  int sig = si->signo;
  queued_last = false;
  if (sig == 0)
    return 0;
  uint64_t f = irq_save();
  if (!prepare_signal(t->proc, sig, t)) {
    irq_restore(f);
    return 0;
  }
  int r = queue_signal(&t->pending, si);
  if (!(t->blocked & SIGBIT(sig)) || sig == SIGKILL)
    signal_wake(t, sig == SIGKILL);
  irq_restore(f);
  return r;
}

void signal_simple(struct proc *p, int sig) {
  struct siginfo si;
  memset(&si, 0, sizeof(si));
  si.signo = sig;
  si.code = SI_KERNEL;
  send_signal_proc(p, &si);
}

/* a synchronous fault in the current thread: cannot be blocked or ignored */
void force_signal(int sig, int code, uint64_t addr, int trapno, uint64_t err) {
  struct proc *p = cur_proc();
  (void)trapno;
  (void)err;
  struct k_sigaction *ka = &p->sigact[sig - 1];
  if (ka->handler == SIG_IGN || (current->blocked & SIGBIT(sig))) {
    ka->handler = SIG_DFL;
    current->blocked &= ~SIGBIT(sig);
  }
  struct siginfo si;
  memset(&si, 0, sizeof(si));
  si.signo = sig;
  si.code = code;
  si.u.fault.addr = addr;
  send_signal_task(current, &si);
}

static bool may_signal(struct proc *p, int sig) {
  struct cred *c = &cur_proc()->cred;
  if (c->euid == 0)
    return true;
  if (c->uid == p->cred.uid || c->uid == p->cred.suid || c->euid == p->cred.uid ||
      c->euid == p->cred.suid)
    return true;
  return sig == SIGCONT && p->sid == cur_proc()->sid;
}

static void fill_user_info(struct siginfo *si, int sig, int code) {
  memset(si, 0, sizeof(*si));
  si->signo = sig;
  si->code = code;
  si->u.kill.pid = cur_proc()->pid;
  si->u.kill.uid = cur_proc()->cred.uid;
}

int kill_pgrp(int pgid, int sig, bool check) {
  struct siginfo si;
  if (check)
    fill_user_info(&si, sig, SI_USER);
  else {
    /* from the kernel (a terminal, an orphaned group): maybe in an
       interrupt, where there is no current process */
    memset(&si, 0, sizeof(si));
    si.signo = sig;
    si.code = SI_KERNEL;
  }
  bool found = false, sent = false;
  list_for_each_safe(q, n, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->pgid != pgid || p->state != P_ALIVE)
      continue;
    found = true;
    if (check && !may_signal(p, sig))
      continue;
    sent = true;
    send_signal_proc(p, &si);
  }
  return !found ? -ESRCH : !sent ? -EPERM : 0;
}

long sys_kill(long pid, long sig) {
  if (sig < 0 || sig > NSIG)
    return -EINVAL;
  struct siginfo si;
  fill_user_info(&si, (int)sig, SI_USER);
  if (pid > 0) {
    struct proc *p = find_proc((pid_t)pid);
    if (!p || p->state != P_ALIVE) {
      struct task *t = find_task((pid_t)pid);
      p = t ? t->proc : NULL;
    }
    if (!p || p->state == P_DEAD)
      return -ESRCH;
    if (!may_signal(p, (int)sig))
      return -EPERM;
    return sig ? send_signal_proc(p, &si) : 0;
  }
  if (pid == 0)
    return sig ? kill_pgrp(cur_proc()->pgid, (int)sig, true) : 0;
  if (pid < -1)
    return kill_pgrp((int)-pid, (int)sig, true);
  /* -1: everything but init and ourselves */
  bool found = false;
  list_for_each_safe(q, n, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p == init_proc || p == cur_proc() || p->state != P_ALIVE || !may_signal(p, (int)sig))
      continue;
    found = true;
    if (sig)
      send_signal_proc(p, &si);
  }
  return found ? 0 : -ESRCH;
}

static long do_tkill(long tgid, long tid, long sig) {
  if (sig < 0 || sig > NSIG || tid <= 0)
    return -EINVAL;
  struct task *t = find_task((pid_t)tid);
  if (!t || !t->proc || (tgid > 0 && t->proc->pid != tgid) || t->exiting)
    return -ESRCH;
  if (!may_signal(t->proc, (int)sig))
    return -EPERM;
  struct siginfo si;
  fill_user_info(&si, (int)sig, SI_TKILL);
  return sig ? send_signal_task(t, &si) : 0;
}

long sys_tkill(long tid, long sig) { return do_tkill(-1, tid, sig); }
long sys_tgkill(long tgid, long tid, long sig) {
  if (tgid <= 0)
    return -EINVAL;
  return do_tkill(tgid, tid, sig);
}

/* the siginfos queued for a process: in its own queue and in its threads'
 * (RLIMIT_SIGPENDING, sysconf(_SC_SIGQUEUE_MAX), limits them) */
static uint64_t queued_count(struct proc *p) {
  uint64_t n = 0;
  uint64_t f = irq_save();
  list_for_each(a, &p->pending.queue) n++;
  list_for_each(q, &p->threads) {
    struct task *t = list_entry(q, struct task, thread_link);
    list_for_each(b, &t->pending.queue) n++;
  }
  irq_restore(f);
  return n;
}

long sys_rt_sigqueueinfo(long pid, long sig, long uinfo) {
  struct siginfo si;
  if (copy_from_user(&si, (void *)uinfo, sizeof(si)))
    return -EFAULT;
  if (sig < 0 || sig > NSIG) /* 0: only the checks (POSIX sigqueue) */
    return -EINVAL;
  if ((si.code >= 0 || si.code == SI_TKILL) && cur_proc()->pid != pid)
    return -EPERM;
  si.signo = (int)sig;
  struct proc *p = find_proc((pid_t)pid);
  if (!p || p->state == P_DEAD)
    return -ESRCH;
  if (!may_signal(p, (int)sig))
    return -EPERM;
  /* sigqueue(): EAGAIN when the signals already queued for the receiver
   * reach the limit */
  if (sig && si.code == SI_QUEUE && queued_count(p) >= cur_proc()->rlim[RLIMIT_SIGPENDING].cur)
    return -EAGAIN;
  return sig ? send_signal_proc(p, &si) : 0;
}

long sys_rt_tgsigqueueinfo(long tgid, long tid, long sig, long uinfo) {
  struct siginfo si;
  if (copy_from_user(&si, (void *)uinfo, sizeof(si)))
    return -EFAULT;
  if (sig < 0 || sig > NSIG)
    return -EINVAL;
  if ((si.code >= 0 || si.code == SI_TKILL) && cur_proc()->pid != tgid)
    return -EPERM;
  struct task *t = find_task((pid_t)tid);
  if (!t || t->proc->pid != tgid)
    return -ESRCH;
  if (!may_signal(t->proc, (int)sig))
    return -EPERM;
  si.signo = (int)sig;
  if (sig && si.code == SI_QUEUE && queued_count(t->proc) >= cur_proc()->rlim[RLIMIT_SIGPENDING].cur)
    return -EAGAIN;
  return sig ? send_signal_task(t, &si) : 0;
}

/* ---- delivery ---- */

static int dequeue_from(struct sigpending *sp, sigset_t allowed, struct siginfo *si) {
  sigset_t s = sp->set & allowed;
  if (!s)
    return 0;
  /* synchronous faults first, then the lowest number */
  sigset_t sync = s & (SIGBIT(SIGSEGV) | SIGBIT(SIGBUS) | SIGBIT(SIGILL) | SIGBIT(SIGTRAP) |
                       SIGBIT(SIGFPE) | SIGBIT(SIGSYS));
  if (sync)
    s = sync;
  int sig = 1;
  while (!(s & SIGBIT(sig)))
    sig++;
  bool found = false, more = false;
  list_for_each_safe(p, n, &sp->queue) {
    struct sigqueue *q = list_entry(p, struct sigqueue, link);
    if (q->info.signo != sig)
      continue;
    if (found) {
      more = true;
      break;
    }
    *si = q->info;
    list_del(p);
    kfree(q);
    found = true;
  }
  if (!found) {
    memset(si, 0, sizeof(*si));
    si->signo = sig;
    si->code = SI_KERNEL;
  }
  if (!more)
    sp->set &= ~SIGBIT(sig);
  return sig;
}

int dequeue_signal(struct task *t, sigset_t allowed, struct siginfo *si) {
  int s = dequeue_from(&t->pending, allowed, si);
  if (!s)
    s = dequeue_from(&t->proc->pending, allowed, si);
  if (s && si->code == SI_TIMER)
    timer_signal_taken(t->proc, si);
  return s;
}

static bool pgrp_orphaned(pid_t pgid) {
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state != P_ALIVE || p->pgid != pgid)
      continue;
    struct proc *pp = p->parent;
    if (pp != p && pp->pgid != pgid && pp->sid == p->sid)
      return false;
  }
  return true;
}

static bool pgrp_has_stopped(pid_t pgid) {
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state == P_ALIVE && p->pgid == pgid && p->stopped)
      return true;
  }
  return false;
}

static void orphan_hup(pid_t pgid) {
  if (pgrp_orphaned(pgid) && pgrp_has_stopped(pgid)) {
    kill_pgrp(pgid, SIGHUP, false);
    kill_pgrp(pgid, SIGCONT, false);
  }
}

/* p is exiting: its process group, and its children's, may become orphaned */
void orphan_check_exit(struct proc *p) {
  pid_t own = p->pgid;
  p->state = P_ZOMBIE; /* no longer counts as a member */
  if (p->parent->pgid != own && p->parent->sid == p->sid)
    orphan_hup(own);
  list_for_each(c, &p->children) {
    struct proc *ch = list_entry(c, struct proc, sibling);
    if (ch->pgid != own && ch->sid == p->sid)
      orphan_hup(ch->pgid);
  }
  p->state = P_ALIVE;
}

static void do_signal_stop(int sig) {
  struct proc *p = cur_proc();
  if (!p->stopped) {
    p->stopped = true;
    p->stop_sig = sig;
    p->report = CLD_STOPPED;
    struct proc *pp = p->parent;
    if (pp != p) {
      if (!(pp->sigact[SIGCHLD - 1].flags & SA_NOCLDSTOP)) {
        struct siginfo si;
        memset(&si, 0, sizeof(si));
        si.signo = SIGCHLD;
        si.code = CLD_STOPPED;
        si.u.chld.pid = p->pid;
        si.u.chld.uid = p->cred.uid;
        si.u.chld.status = sig;
        send_signal_proc(pp, &si);
      }
      wake_up(&pp->child_wait);
    }
    list_for_each(q, &p->threads) {
      struct task *t = list_entry(q, struct task, thread_link);
      if (t != current)
        signal_wake(t, false);
    }
  }
}

static void stop_here(void) {
  struct proc *p = cur_proc();
  while (p->stopped && !p->group_exit && !(current->pending.set & SIGBIT(SIGKILL))) {
    current->state = T_STOPPED;
    schedule();
  }
}

struct sigframe {
  uint64_t pretcode;
  /* struct ucontext */
  uint64_t uc_flags, uc_link;
  struct sigaltstack uc_stack;
  uint64_t gregs[23]; /* sigcontext up to cr2 */
  uint64_t fpstate;
  uint64_t reserved1[8];
  uint64_t uc_sigmask;
  struct siginfo info;
};

/* sigcontext order */
enum { R8, R9, R10, R11, R12, R13, R14, R15, RDI, RSI, RBP, RBX, RDX, RAX, RCX, RSP, RIP, EFL,
       CSGSFS, ERR, TRAPNO, OLDMASK, CR2 };

static bool on_altstack(uint64_t sp) {
  struct sigaltstack *a = &current->altstack;
  return !(a->flags & SS_DISABLE) && sp > a->sp && sp - a->sp <= a->size;
}

static int setup_frame(int sig, struct k_sigaction *ka, struct siginfo *si, struct regs *r,
                       sigset_t oldmask) {
  uint64_t sp = r->rsp;
  if ((ka->flags & SA_ONSTACK) && !(current->altstack.flags & SS_DISABLE) && !on_altstack(sp))
    sp = current->altstack.sp + current->altstack.size;
  else
    sp -= 128; /* red zone */
  sp = (sp - 512) & ~63UL;
  uint64_t fp = sp;
  fxsave(current->fpu);
  if (copy_to_user((void *)fp, current->fpu, 512))
    return -EFAULT;
  sp -= sizeof(struct sigframe);
  sp = ((sp + 8) & ~15UL) - 8;
  struct sigframe f;
  memset(&f, 0, sizeof(f));
  f.pretcode = ka->restorer;
  f.uc_flags = 6; /* UC_SIGCONTEXT_SS | UC_STRICT_RESTORE_SS */
  f.uc_stack.sp = current->altstack.sp;
  f.uc_stack.size = current->altstack.size;
  f.uc_stack.flags = (current->altstack.flags & SS_DISABLE) ? SS_DISABLE : on_altstack(r->rsp) ? SS_ONSTACK : 0;
  uint64_t *g = f.gregs;
  g[R8] = r->r8, g[R9] = r->r9, g[R10] = r->r10, g[R11] = r->r11;
  g[R12] = r->r12, g[R13] = r->r13, g[R14] = r->r14, g[R15] = r->r15;
  g[RDI] = r->rdi, g[RSI] = r->rsi, g[RBP] = r->rbp, g[RBX] = r->rbx;
  g[RDX] = r->rdx, g[RAX] = r->rax, g[RCX] = r->rcx, g[RSP] = r->rsp;
  g[RIP] = r->rip, g[EFL] = r->rflags;
  g[CSGSFS] = USER_CS | (uint64_t)USER_DS << 48;
  g[ERR] = r->vector == 14 ? r->err : 0;
  g[TRAPNO] = r->vector < 32 ? r->vector : 0;
  g[OLDMASK] = oldmask;
  g[CR2] = sig == SIGSEGV || sig == SIGBUS ? si->u.fault.addr : 0;
  f.fpstate = fp;
  f.uc_sigmask = oldmask;
  f.info = *si;
  if (copy_to_user((void *)sp, &f, sizeof(f)))
    return -EFAULT;
  r->rsp = sp;
  r->rip = ka->handler;
  r->rdi = (uint64_t)sig;
  r->rsi = sp + offsetof(struct sigframe, info);
  r->rdx = sp + offsetof(struct sigframe, uc_flags);
  r->rax = 0;
  r->rflags &= ~0x500UL; /* DF, TF */
  r->cs = USER_CS;
  r->ss = USER_DS;
  return 0;
}

long sys_rt_sigreturn(void) {
  struct regs *r = current->regs;
  uint64_t sp = r->rsp - 8;
  struct sigframe f;
  if (copy_from_user(&f, (void *)sp, sizeof(f))) {
    force_signal(SIGSEGV, SI_KERNEL, 0, 0, 0);
    return r->rax;
  }
  uint64_t *g = f.gregs;
  r->r8 = g[R8], r->r9 = g[R9], r->r10 = g[R10], r->r11 = g[R11];
  r->r12 = g[R12], r->r13 = g[R13], r->r14 = g[R14], r->r15 = g[R15];
  r->rdi = g[RDI], r->rsi = g[RSI], r->rbp = g[RBP], r->rbx = g[RBX];
  r->rdx = g[RDX], r->rax = g[RAX], r->rcx = g[RCX], r->rsp = g[RSP];
  r->rip = g[RIP];
  r->rflags = (r->rflags & ~0x40dd5UL) | (g[EFL] & 0x40dd5UL);
  r->cs = USER_CS;
  r->ss = USER_DS;
  r->vector = 0; /* not a system call any more: no restart */
  current->blocked = f.uc_sigmask & ~UNBLOCKABLE;
  if (f.fpstate) {
    uint8_t fx[512];
    if (copy_from_user(fx, (void *)f.fpstate, 512) == 0) {
      uint32_t mxcsr;
      memcpy(&mxcsr, fx + 24, 4);
      mxcsr &= 0xffff;
      memcpy(fx + 24, &mxcsr, 4);
      memcpy(current->fpu, fx, 512);
      fxrstor(current->fpu);
    }
  } else {
    fninit_sse();
    fxsave(current->fpu);
  }
  /* sigaltstack as restored by the frame */
  if (!on_altstack(r->rsp) && !(f.uc_stack.flags & SS_DISABLE) && f.uc_stack.size >= 2048) {
    current->altstack.sp = f.uc_stack.sp;
    current->altstack.size = f.uc_stack.size;
    current->altstack.flags = 0;
  }
  return (long)r->rax;
}

static bool is_restart(uint64_t rax) {
  long v = (long)rax;
  return v == -ERESTARTSYS || v == -ERESTARTNOINTR || v == -ERESTARTNOHAND ||
         v == -ERESTART_RESTARTBLOCK;
}

bool needs_signal_work(struct regs *r) {
  return signal_pending(current) || current->restore_mask ||
         (r->vector == VEC_SYSCALL && is_restart(r->rax));
}

static void restart_syscall(struct regs *r) {
  r->rax = r->err;
  r->rip -= 2;
}

void do_signals(struct regs *r) {
  struct proc *p = cur_proc();
  for (;;) {
    if (p->group_exit || (current->pending.set & SIGBIT(SIGKILL)))
      do_exit(p->group_exit ? p->group_status : SIGKILL);
    if (p->stopped) {
      stop_here();
      continue;
    }
    struct siginfo si;
    int sig = dequeue_signal(current, ~current->blocked, &si);
    if (!sig)
      break;
    struct k_sigaction *ka = &p->sigact[sig - 1];
    if (ka->handler == SIG_IGN)
      continue;
    if (ka->handler == SIG_DFL) {
      if (default_ignored(sig))
        continue;
      if (SIGBIT(sig) & STOP_SIGS) {
        if (sig != SIGSTOP && pgrp_orphaned(p->pgid))
          continue;
        do_signal_stop(sig);
        continue;
      }
      int st = sig | ((CORE_SIGS & SIGBIT(sig)) && p->rlim[RLIMIT_CORE].cur ? 0x80 : 0);
      sti();
      do_group_exit(st);
    }
    /* a handler */
    if (r->vector == VEC_SYSCALL) {
      long v = (long)r->rax;
      if (v == -ERESTARTNOHAND || v == -ERESTART_RESTARTBLOCK ||
          (v == -ERESTARTSYS && !(ka->flags & SA_RESTART))) {
        r->rax = (uint64_t)-EINTR;
        current->sleep_restart = false;
      }
      else if (v == -ERESTARTSYS || v == -ERESTARTNOINTR)
        restart_syscall(r);
      r->vector = 0;
    }
    sigset_t old = current->restore_mask ? current->saved_mask : current->blocked;
    current->restore_mask = false;
    if (!ka->restorer || setup_frame(sig, ka, &si, r, old)) {
      sti();
      do_group_exit(SIGSEGV);
    }
    current->blocked |= ka->mask;
    if (!(ka->flags & SA_NODEFER))
      current->blocked |= SIGBIT(sig);
    current->blocked &= ~UNBLOCKABLE;
    if (ka->flags & SA_RESETHAND)
      ka->handler = SIG_DFL;
    return;
  }
  /* no handler ran */
  if (r->vector == VEC_SYSCALL && is_restart(r->rax)) {
    restart_syscall(r);
    r->vector = 0;
  }
  if (current->restore_mask) {
    current->restore_mask = false;
    current->blocked = current->saved_mask;
  }
}

void signals_exec_reset(struct proc *p) {
  for (int i = 0; i < NSIG; i++) {
    struct k_sigaction *ka = &p->sigact[i];
    if (ka->handler != SIG_IGN)
      ka->handler = SIG_DFL;
    ka->flags = 0;
    ka->mask = 0;
    ka->restorer = 0;
  }
  current->altstack.flags = SS_DISABLE;
  current->altstack.sp = current->altstack.size = 0;
}

/* ---- system calls ---- */

long sys_rt_sigaction(long sig, long unew, long uold, long size) {
  if (size != 8)
    return -EINVAL;
  if (sig < 1 || sig > NSIG)
    return -EINVAL;
  struct proc *p = cur_proc();
  struct k_sigaction n, o = p->sigact[sig - 1];
  if (unew) {
    if (sig == SIGKILL || sig == SIGSTOP)
      return -EINVAL;
    if (copy_from_user(&n, (void *)unew, sizeof(n)))
      return -EFAULT;
  }
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  if (unew) {
    n.mask &= ~UNBLOCKABLE;
    uint64_t f = irq_save();
    p->sigact[sig - 1] = n;
    /* setting a signal to be ignored discards it (POSIX) */
    if (sig_ignored(p, (int)sig)) {
      sp_remove(p, &p->pending, SIGBIT(sig));
      list_for_each(q, &p->threads) sp_remove(p, &list_entry(q, struct task, thread_link)->pending, SIGBIT(sig));
    }
    irq_restore(f);
  }
  return 0;
}

long sys_rt_sigprocmask(long how, long uset, long uold, long size) {
  if (size != 8)
    return -EINVAL;
  sigset_t old = current->blocked, s;
  if (uset) {
    if (copy_from_user(&s, (void *)uset, 8))
      return -EFAULT;
    s &= ~UNBLOCKABLE;
    switch (how) {
    case SIG_BLOCK:
      current->blocked |= s;
      break;
    case SIG_UNBLOCK:
      current->blocked &= ~s;
      break;
    case SIG_SETMASK:
      current->blocked = s;
      break;
    default:
      return -EINVAL;
    }
  }
  if (uold && copy_to_user((void *)uold, &old, 8))
    return -EFAULT;
  return 0;
}

long sys_rt_sigpending(long uset, long size) {
  if (size > 8 || size < 0)
    return -EINVAL;
  sigset_t s = (current->pending.set | cur_proc()->pending.set) & current->blocked;
  return copy_to_user((void *)uset, &s, (size_t)size) ? -EFAULT : 0;
}

long sys_rt_sigsuspend(long uset, long size) {
  if (size != 8)
    return -EINVAL;
  sigset_t s;
  if (copy_from_user(&s, (void *)uset, 8))
    return -EFAULT;
  current->saved_mask = current->blocked;
  current->restore_mask = true;
  current->blocked = s & ~UNBLOCKABLE;
  uint64_t f = irq_save();
  while (!signal_pending(current))
    sleep_on(NULL, 0, true);
  irq_restore(f);
  return -ERESTARTNOHAND;
}

long sys_pause(void) {
  uint64_t f = irq_save();
  while (!signal_pending(current))
    sleep_on(NULL, 0, true);
  irq_restore(f);
  return -ERESTARTNOHAND;
}

long sys_rt_sigtimedwait(long uset, long uinfo, long uts, long size) {
  if (size != 8)
    return -EINVAL;
  sigset_t s;
  if (copy_from_user(&s, (void *)uset, 8))
    return -EFAULT;
  s &= ~UNBLOCKABLE;
  uint64_t deadline = 0;
  if (uts) {
    struct timespec64 ts;
    if (copy_from_user(&ts, (void *)uts, sizeof(ts)))
      return -EFAULT;
    if (ts.nsec < 0 || ts.nsec >= 1000000000 || ts.sec < 0)
      return -EINVAL;
    deadline = sat_add(monotonic_ns(), ts_to_ns(ts.sec, ts.nsec));
    if (deadline == 0)
      deadline = 1;
  }
  struct siginfo si;
  uint64_t f = irq_save();
  int sig;
  for (;;) {
    sig = dequeue_signal(current, s, &si);
    if (sig)
      break;
    /* other pending, unblocked signals interrupt the wait */
    sigset_t saved = current->blocked;
    current->blocked &= ~s;
    int e = sleep_on(NULL, deadline, true);
    current->blocked = saved;
    if (e == -ETIMEDOUT) {
      irq_restore(f);
      return -EAGAIN;
    }
    if (e == -EINTR) {
      sig = dequeue_signal(current, s, &si);
      if (sig)
        break;
      irq_restore(f);
      return -EINTR;
    }
  }
  irq_restore(f);
  if (uinfo && copy_to_user((void *)uinfo, &si, sizeof(si)))
    return -EFAULT;
  return sig;
}

long sys_sigaltstack(long unew, long uold) {
  struct sigaltstack o = current->altstack, n;
  struct regs *r = current->regs;
  if (on_altstack(r->rsp))
    o.flags = SS_ONSTACK;
  else if (current->altstack.flags & SS_DISABLE)
    o.flags = SS_DISABLE;
  else
    o.flags = 0;
  if (unew) {
    if (copy_from_user(&n, (void *)unew, sizeof(n)))
      return -EFAULT;
    if (on_altstack(r->rsp))
      return -EPERM;
    if (n.flags & ~(SS_DISABLE | (int)0x80000000))
      return -EINVAL;
    if (n.flags & SS_DISABLE) {
      current->altstack.flags = SS_DISABLE;
      current->altstack.sp = current->altstack.size = 0;
    } else {
      if (n.size < 2048)
        return -ENOMEM;
      current->altstack.sp = n.sp;
      current->altstack.size = n.size;
      current->altstack.flags = 0;
    }
  }
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  return 0;
}

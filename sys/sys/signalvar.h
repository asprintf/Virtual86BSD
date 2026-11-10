/* signalvar.h - signals inside the kernel. */
#ifndef _SYS_SIGNALVAR_H_
#define _SYS_SIGNALVAR_H_

#include <sys/systm.h>
#include <sys/signal.h>

struct k_sigaction {
  uint64_t handler;
  uint64_t flags;
  uint64_t restorer;
  sigset_t mask;
};

struct sigqueue {
  struct list link;
  struct siginfo info;
};

struct task;
struct proc;
struct regs;

int send_signal_proc(struct proc *p, const struct siginfo *si);
int send_signal_task(struct task *t, const struct siginfo *si);
int kill_pgrp(int pgid, int sig, bool check_perm);
void force_signal(int sig, int code, uint64_t addr, int trapno, uint64_t err);
void signal_simple(struct proc *p, int sig); /* from the kernel, SI_KERNEL */
bool signal_pending(struct task *t);
void do_signals(struct regs *r);
bool needs_signal_work(struct regs *r);
int dequeue_signal(struct task *t, sigset_t allowed, struct siginfo *si);
bool signal_was_queued(void);
/* a POSIX timer's signal leaves the queue (delivered, accepted, or
 * discarded): its overrun count is fixed, and the timer may queue again */
void timer_signal_taken(struct proc *p, struct siginfo *si);
bool sig_ignored(struct proc *p, int sig);
void signals_exec_reset(struct proc *p);

#endif

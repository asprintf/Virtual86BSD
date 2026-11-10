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

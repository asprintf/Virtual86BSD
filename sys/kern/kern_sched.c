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

/* sched.c - round-robin scheduler, wait queues, sleeping with timeouts,
   kernel threads.  The kernel is not preemptive: a task switches only in
   schedule(), which runs with interrupts off, by the processor that holds
   the kernel lock (arch lock_machdep.c).  Every processor has its own idle
   task and waits without the lock until a waker kicks it. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define KSTACK_ORDER 2 /* 16 KiB */
void reset_pids(void);
#define SLICE (10 * HZ / 1000 ? 10 * HZ / 1000 : 1) /* 10 ms */

static struct list runq = LIST_INIT(runq);
static struct list timed = LIST_INIT(timed);
int ncpu_sched = 1; /* processors in the scheduler */

/* real-time throttling, as Linux's sched_rt_runtime_us: real-time tasks
   get at most 950 ms of every second on a processor (ci_rt_ticks) */
#define RT_LIMIT (950 * HZ / 1000)
static bool rt_throttled(const struct cpu_info *ci) { return ci->ci_rt_ticks >= RT_LIMIT; }

/* ---- SCHED_SPORADIC (POSIX XSH 2.8.4) ----
   A sporadic server runs as SCHED_FIFO at sched_priority while it has
   execution capacity (budget) left, and at sched_ss_low_priority when not.
   What it runs at sched_priority is charged (per tick, 1 ms); it is
   "activated" when it becomes runnable at sched_priority, and when it
   blocks there or its capacity runs out, a replenishment of what it used
   since the activation is due one replenishment period after the
   activation (at most sched_ss_max_repl pending: beyond that the amount
   joins the last one).  A replenishment adds the amount back, up to the
   initial budget, and puts a server waiting at the low priority back at
   sched_priority. */
struct ss {
  struct list link;
  struct task *t;
  int hi, lo, max_repl;
  uint64_t period, init; /* ns */
  int64_t budget;
  uint64_t activated, used;
  bool active, low;
  int n;
  struct { uint64_t at, amount; } r[SS_REPL_MAX];
};
static struct list ss_list = LIST_INIT(ss_list);

static void ss_activate(struct ss *s, uint64_t now) {
  if (s->low)
    return;
  s->active = true;
  s->activated = now;
  s->used = 0;
}

static void ss_replenish_at(struct ss *s, uint64_t at, uint64_t amount, uint64_t now) {
  if (!amount)
    return;
  if (at <= now) {
    s->budget = (int64_t)MIN((uint64_t)s->budget + amount, s->init);
    return;
  }
  if (s->n < s->max_repl)
    s->r[s->n].at = at, s->r[s->n].amount = amount, s->n++;
  else
    s->r[s->n - 1].amount += amount;
}

/* the server stops running at sched_priority (blocked or out of budget) */
static void ss_stop(struct ss *s, uint64_t now) {
  if (!s->active)
    return;
  s->active = false;
  ss_replenish_at(s, s->activated + s->period, s->used, now);
  s->used = 0;
}

static void ss_tick(uint64_t now) {
  list_for_each(p, &ss_list) {
    struct ss *s = list_entry(p, struct ss, link);
    bool changed = false;
    while (s->n && s->r[0].at <= now) {
      s->budget = (int64_t)MIN((uint64_t)s->budget + s->r[0].amount, s->init);
      s->n--;
      memmove(&s->r[0], &s->r[1], (size_t)s->n * sizeof(s->r[0]));
      changed = true;
    }
    if (changed && s->low && s->budget > 0) {
      s->low = false;
      if (s->t->state == T_RUNNING)
        ss_activate(s, now);
      task_prio_changed(s->t);
    }
  }
}

static void ss_charge(struct task *t, uint64_t ns, uint64_t now) {
  struct ss *s = t->ss;
  if (!s || s->low || !s->active)
    return;
  s->used += ns;
  s->budget -= (int64_t)ns;
  if (s->budget <= 0) {
    s->budget = 0;
    ss_stop(s, now);
    s->low = true;
    need_resched = true;
  }
}

int ss_set(struct task *t, const struct ss_param *p) {
  uint64_t period = (uint64_t)p->period_s * 1000000000UL + (uint64_t)p->period_ns;
  uint64_t init = (uint64_t)p->budget_s * 1000000000UL + (uint64_t)p->budget_ns;
  if (p->prio < 1 || p->prio > 99 || p->low < 1 || p->low > 99 || p->max_repl < 1 || p->max_repl > SS_REPL_MAX ||
      p->period_s < 0 || p->budget_s < 0 || p->period_ns < 0 || p->period_ns >= 1000000000 || p->budget_ns < 0 ||
      p->budget_ns >= 1000000000 || init == 0 || period < init)
    return -EINVAL;
  struct ss *s = t->ss;
  if (!s) {
    s = kzalloc(sizeof(*s));
    if (!s)
      return -ENOMEM;
    s->t = t;
  }
  uint64_t f = irq_save(); /* (the timer interrupt walks ss_list) */
  if (!t->ss) {
    list_add_tail(&ss_list, &s->link);
    t->ss = s;
  }
  s->hi = p->prio, s->lo = p->low, s->max_repl = p->max_repl;
  s->period = period, s->init = init;
  s->budget = (int64_t)init;
  s->n = 0;
  s->low = false;
  s->active = false;
  if (t->state == T_RUNNING)
    ss_activate(s, monotonic_ns());
  t->policy = SCHED_SPORADIC;
  t->rtprio = p->prio;
  irq_restore(f);
  task_prio_changed(t);
  return 0;
}

void ss_get(struct task *t, struct ss_param *p) {
  memset(p, 0, sizeof(*p));
  struct ss *s = t->ss;
  if (!s)
    return;
  p->prio = s->hi, p->low = s->lo, p->max_repl = s->max_repl;
  p->period_s = (int64_t)(s->period / 1000000000UL), p->period_ns = (int64_t)(s->period % 1000000000UL);
  p->budget_s = (int64_t)(s->init / 1000000000UL), p->budget_ns = (int64_t)(s->init % 1000000000UL);
}

void ss_copy(struct task *dst, struct task *src) {
  if (src->ss) {
    struct ss_param p;
    ss_get(src, &p);
    ss_set(dst, &p);
  }
}

void ss_free(struct task *t) {
  if (t->ss) {
    uint64_t f = irq_save();
    struct ss *s = t->ss;
    list_del(&s->link);
    t->ss = NULL;
    irq_restore(f);
    kfree(s);
  }
}

/* the priority: real-time policies have 1..99, others 0; a priority
   inherited through a PI futex counts too */
int task_prio(struct task *t) {
  int p = (t->policy == 1 || t->policy == 2) ? t->rtprio : 0;
  if (t->policy == SCHED_SPORADIC && t->ss)
    p = t->ss->low ? t->ss->lo : t->ss->hi;
  return t->boost > p ? t->boost : p;
}

/* the priority t counts for on ci: the real-time ones do not while it is throttled */
static int cpu_prio(const struct cpu_info *ci, struct task *t) { return rt_throttled(ci) ? 0 : task_prio(t); }

/* t may run on ci (the run queue has no idle tasks) */
static bool can_run(const struct cpu_info *ci, const struct task *t) {
  return t->cpus_allowed >> ci->ci_index & 1;
}

/* A processor in idle_loop's wait is told by ci_kick and an IPI (the kernel
   lock is held by the caller, which makes the order with idle_loop's checks). */
static void kick_cpu(struct cpu_info *ci) {
  ci->ci_kick = 1;
  ipi_resched(ci);
}

/* This processor when it is idle (we are in an interrupt that woke something),
   else another that is idle and may run t: tell it to look at the run queue. */
static bool kick_idle(struct task *t) {
  struct cpu_info *me = curcpu();
  if (me->ci_curtask && me->ci_curtask->is_idle && can_run(me, t)) {
    me->ci_kick = 1;
    return true;
  }
  if (ncpu_sched > 1) {
    for (int i = 0; i < MAXCPUS; i++) {
      struct cpu_info *ci = cpu_info_list[i];
      if (ci && ci != me && ci->ci_idling && !ci->ci_kick && can_run(ci, t)) {
        kick_cpu(ci);
        return true;
      }
    }
  }
  return false;
}

/* t has become runnable, in the run queue: have a processor take it. Idle
   ones first, then one running something t outranks (equal: or ranks with). */
static void run_somewhere(struct task *t, bool equal) {
  struct cpu_info *me = curcpu();
  if (!me->ci_curtask) {
    me->ci_need_resched = true;
    return;
  }
  if (kick_idle(t))
    return;
  if (can_run(me, t) && (cpu_prio(me, t) > cpu_prio(me, me->ci_curtask) ||
                         (equal && cpu_prio(me, t) == cpu_prio(me, me->ci_curtask)))) {
    me->ci_need_resched = true;
    return;
  }
  for (int i = 0; ncpu_sched > 1 && i < MAXCPUS; i++) {
    struct cpu_info *ci = cpu_info_list[i];
    if (!ci || ci == me || !can_run(ci, t) || !ci->ci_curtask || ci->ci_curtask->is_idle)
      continue;
    int a = cpu_prio(ci, t), b = cpu_prio(ci, ci->ci_curtask);
    if (a > b || (equal && a == b)) {
      ci->ci_need_resched = true;
      ipi_resched(ci);
      return;
    }
  }
}

void task_prio_changed(struct task *t) {
  if (t->state != T_RUNNING)
    return;
  if (t->oncpu >= 0) {
    /* running: a lower priority may let another run */
    struct cpu_info *ci = cpu_info_list[t->oncpu];
    ci->ci_need_resched = true;
    if (ci != curcpu())
      ipi_resched(ci);
  } else {
    run_somewhere(t, false);
  }
}

static void runq_add(struct task *t) {
  KASSERT(!t->is_idle);
  if (t->run_link.next == &t->run_link)
    list_add_tail(&runq, &t->run_link);
}

/* put t in the run queue, and wake a processor that is idle for it */
void make_runnable(struct task *t) {
  KASSERT(!t->is_idle);
  if (t->run_link.next == &t->run_link) {
    list_add_tail(&runq, &t->run_link);
    kick_idle(t);
  }
}

void wake_task(struct task *t) {
  uint64_t f = irq_save();
  if (t->state == T_SLEEPING || t->state == T_UNINTERRUPTIBLE) {
    t->state = T_RUNNING;
    if (t->ss)
      ss_activate(t->ss, monotonic_ns());
    runq_add(t);
    run_somewhere(t, true);
  }
  irq_restore(f);
}

void wake_up(struct waitq *q) {
  uint64_t f = irq_save();
  list_for_each_safe(p, n, &q->head) {
    struct task *t = list_entry(p, struct task, wait_link);
    wake_task(t);
  }
  irq_restore(f);
}

void wake_up_one(struct waitq *q) {
  uint64_t f = irq_save();
  list_for_each(p, &q->head) {
    struct task *t = list_entry(p, struct task, wait_link);
    if (t->state == T_SLEEPING || t->state == T_UNINTERRUPTIBLE) {
      wake_task(t);
      break;
    }
  }
  irq_restore(f);
}

static void finish_switch(void) {
  struct cpu_info *ci = curcpu();
  struct task *t = ci->ci_dead;
  if (t && t != current) {
    ci->ci_dead = NULL;
    task_free(t);
  }
}

void schedule_tail(void) {
  finish_switch();
}

void schedule(void) {
  struct cpu_info *ci = curcpu();
  struct task *prev = current;
  KASSERT(bkl_held());
  need_resched = false;
  if (prev->state == T_RUNNING) {
    if (!prev->is_idle)
      runq_add(prev);
  } else if (prev->ss)
    ss_stop(prev->ss, monotonic_ns()); /* it blocks */
  /* the first of the highest priority (FIFO among equals) */
  struct task *next = ci->ci_idle;
  int best = -1;
  bool throttled = rt_throttled(ci);
  list_for_each(p, &runq) {
    struct task *t = list_entry(p, struct task, run_link);
    if (!can_run(ci, t))
      continue;
    int pr = throttled ? 0 : task_prio(t);
    if (pr > best) {
      best = pr;
      next = t;
    }
  }
  if (!next->is_idle)
    list_del(&next->run_link);
  if (next == prev)
    return;
  if (prev->state == T_RUNNING && !prev->is_idle)
    kick_idle(prev); /* (it stays in the run queue) */
  KASSERT(next->oncpu == -1);
  KASSERT(next->is_idle ? next == ci->ci_idle : can_run(ci, next));
  prev->oncpu = -1;
  next->oncpu = ci->ci_index;
  next->slice = SLICE;
  if (!prev->is_kthread && prev->fpu)
    fxsave(prev->fpu);
  current = next;
  struct mm *nm = next->proc ? next->proc->mm : NULL; /* (tasks without an address space: the kernel's tables) */
  mm_switch(nm);
  KASSERT(ci->ci_mm == nm && (!nm || (nm->cpus >> ci->ci_index & 1)));
  set_kernel_stack((uint64_t)next->kstack + (PAGE_SIZE << KSTACK_ORDER));
  if (!next->is_kthread && next->fpu) {
    fxrstor(next->fpu);
    wrmsr(MSR_FS_BASE, next->fsbase);
  }
  switch_stack(&prev->ksp, next->ksp);
  finish_switch();
}

void yield(void) {
  uint64_t f = irq_save();
  schedule();
  irq_restore(f);
}

/* Sleep on q until woken, until the deadline (monotonic ns, 0 = none) or,
   if interruptible, until a signal is pending.  Interrupts must be off
   (the caller checked its condition with them off); they stay off.
   Returns 0, -ETIMEDOUT or -EINTR. */
int sleep_on(struct waitq *q, uint64_t deadline, bool interruptible) {
  struct task *t = current;
  if (interruptible && signal_pending(t))
    return -EINTR;
  if (deadline && monotonic_ns() >= deadline)
    return -ETIMEDOUT;
  t->state = interruptible ? T_SLEEPING : T_UNINTERRUPTIBLE;
  t->timed_out = false;
  if (q) {
    t->waitq = q;
    list_add_tail(&q->head, &t->wait_link);
  }
  if (deadline) {
    t->wake_at = deadline;
    list_add_tail(&timed, &t->timer_link);
  }
  schedule();
  if (q) {
    list_del(&t->wait_link);
    t->waitq = NULL;
  }
  if (deadline) {
    list_del(&t->timer_link);
    t->wake_at = 0;
  }
  if (t->timed_out)
    return -ETIMEDOUT;
  if (interruptible && signal_pending(t))
    return -EINTR;
  return 0;
}

int sleep_ns(uint64_t ns) {
  uint64_t deadline = sat_add(monotonic_ns(), ns);
  uint64_t f = irq_save();
  int r;
  while ((r = sleep_on(NULL, deadline, true)) == 0)
    ;
  irq_restore(f);
  return r == -ETIMEDOUT ? 0 : r;
}

/* the clock interrupt of the boot processor, with the frame of what it
   interrupted: time, sleeps with a timeout, interval timers, and its own tick */
void timer_tick(bool user) {
  if (!list_empty(&ss_list))
    ss_tick(monotonic_ns());
  cpu_tick(user);
  jiffies++;
  uint64_t now = monotonic_ns();
  list_for_each_safe(p, n, &timed) {
    struct task *t = list_entry(p, struct task, timer_link);
    if (t->wake_at && now >= t->wake_at) {
      t->timed_out = true;
      wake_task(t);
    }
  }
  itimer_tick();
}

/* every processor's tick, with the frame of what it interrupted: the time
   slice, the throttle and who gets the time */
void cpu_tick(bool user) {
  struct cpu_info *ci = curcpu();
  uint64_t sec = jiffies / HZ;
  if (sec != ci->ci_rt_sec) {
    if (rt_throttled(ci))
      need_resched = true; /* the throttle ends: real-time priorities count again */
    ci->ci_rt_sec = sec;
    ci->ci_rt_ticks = 0;
  }
  struct task *c = current;
  if (!c || c->is_idle)
    return;
  if (c->ss)
    ss_charge(c, 1000000000UL / HZ, monotonic_ns());
  if (!rt_throttled(ci) && task_prio(c) > 0 && ++ci->ci_rt_ticks >= RT_LIMIT)
    need_resched = true;
  if (user) {
    c->utime++;
    if (c->proc)
      c->proc->utime++;
  } else {
    c->stime++;
    if (c->proc)
      c->proc->stime++;
  }
  /* SCHED_FIFO (and a sporadic server) runs until it blocks or yields,
     but not while real-time tasks are throttled (they are not then) */
  if (--c->slice <= 0 && (rt_throttled(ci) || (c->policy != 1 && c->policy != SCHED_SPORADIC)))
    need_resched = true;
}

/* on every return to user mode, with interrupts off */
void return_to_user(struct regs *r) {
  KASSERT(curcpu()->ci_mm == (current->proc ? current->proc->mm : NULL));
  for (;;) {
    if (need_resched) {
      schedule();
      continue;
    }
    if (needs_signal_work(r)) {
      do_signals(r);
      if (signal_pending(current) || need_resched)
        continue;
    }
    return;
  }
}

static bool runq_has_work(const struct cpu_info *ci) {
  list_for_each(p, &runq) {
    if (can_run(ci, list_entry(p, struct task, run_link)))
      return true;
  }
  return false;
}

/* The idle task of a processor, with the kernel lock and interrupts off.
   With nothing to run the lock is let go and the processor halts until it is
   kicked (ci_idling is set before the lock goes, so a waker that finds the run
   queue changed after our look sees it; clearing ci_kick under the lock). */
static NORETURN void idle_loop(struct cpu_info *ci) {
  for (;;) {
    ci->ci_need_resched = false;
    ci->ci_kick = 0;
    if (runq_has_work(ci)) {
      schedule();
      continue;
    }
    ci->ci_idling = 1;
    bkl_unlock();
    while (!ci->ci_kick)
      sti_hlt();
    ci->ci_idling = 0;
    bkl_lock();
  }
}

static long idle_fn(void *arg) {
  (void)arg;
  cli();
  idle_loop(curcpu());
}

/* an application processor, on its idle task's stack: it joins the scheduler */
void ap_idle(void) {
  bkl_lock();
  ncpu_sched++;
  idle_loop(curcpu());
}

/* the tick of an idle processor needs no lock: it only looks for work that was
   queued without a kick */
void idle_tick(void) {
  if (!list_empty(&runq))
    curcpu()->ci_kick = 1;
}

static struct task *task_kernel(long (*fn)(void *), void *arg) {
  struct task *t = task_alloc(NULL);
  if (!t)
    return NULL;
  t->is_kthread = true;
  kproc_attach(t);
  /* the stack switch_stack pops: r15 r14 r13 r12 rbp rbx, return address */
  uint64_t *sp = (uint64_t *)((char *)t->kstack + (PAGE_SIZE << KSTACK_ORDER));
  *--sp = 0;
  *--sp = (uint64_t)kthread_start;
  *--sp = 0;                /* rbx */
  *--sp = 0;                /* rbp */
  *--sp = (uint64_t)fn;     /* r12 */
  *--sp = (uint64_t)arg;    /* r13 */
  *--sp = 0;                /* r14 */
  *--sp = 0;                /* r15 */
  t->ksp = (uint64_t)sp;
  return t;
}

struct task *kthread_create(const char *name, long (*fn)(void *), void *arg) {
  (void)name;
  struct task *t = task_kernel(fn, arg);
  if (!t)
    return NULL;
  uint64_t f = irq_save();
  t->state = T_RUNNING;
  make_runnable(t);
  irq_restore(f);
  return t;
}

void kthread_exit(long code) {
  (void)code;
  cli();
  current->state = T_ZOMBIE;
  kproc_detach(current);
  list_del(&current->all_link);
  curcpu()->ci_dead = current;
  schedule();
  panic("dead kernel thread ran");
}

/* the boot thread becomes the idle task */
void sched_init(void) {
  static uint64_t boot_ksp;
  proc_init_kernel();
  struct task *idle = task_alloc(NULL);
  idle->tid = 0; /* the idle task is thread 0 of kernel_task; init will be pid 1 */
  reset_pids();
  idle->is_kthread = true;
  idle->is_idle = true;
  kproc_attach(idle);
  idle->state = T_RUNNING;
  idle->oncpu = 0;
  curcpu()->ci_idle = idle;
  current = idle;
  (void)boot_ksp;
}

/* called by the boot thread at the end of kmain: become idle forever */
NORETURN void cpu_idle(void) {
  /* switch onto the idle task's own stack, leaving the boot stack */
  struct task *t = task_kernel(idle_fn, NULL);
  t->tid = 0;
  t->is_kthread = true;
  t->is_idle = true;
  t->state = T_RUNNING;
  t->oncpu = 0;
  struct task *boot = curcpu()->ci_idle;
  boot->oncpu = -1;
  curcpu()->ci_idle = t;
  current = t;
  set_kernel_stack((uint64_t)t->kstack + (PAGE_SIZE << KSTACK_ORDER));
  boot->state = T_ZOMBIE;
  cli();
  kproc_detach(boot);
  list_del(&boot->all_link);
  curcpu()->ci_dead = boot; /* freed by the idle task, once off the boot thread */
  cpu_release();
  switch_stack(&boot->ksp, t->ksp);
  panic("boot thread resumed");
}

int kstack_order(void) { return KSTACK_ORDER; }
uint64_t task_kstack_top(struct task *t) { return (uint64_t)t->kstack + (PAGE_SIZE << KSTACK_ORDER); }
void reset_pids(void);

void task_set_dead(struct task *t) { curcpu()->ci_dead = t; }

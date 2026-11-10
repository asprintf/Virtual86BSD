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
   kernel threads.  One processor; the kernel is not preemptive: a task
   switches only in schedule(), which runs with interrupts off. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

#define KSTACK_ORDER 2 /* 16 KiB */
void reset_pids(void);
#define SLICE (10 * HZ / 1000 ? 10 * HZ / 1000 : 1) /* 10 ms */

struct task *current;
bool need_resched;
static struct list runq = LIST_INIT(runq);
static struct list timed = LIST_INIT(timed);
static struct task *idle_task;
static struct task *dead_task; /* to free once off its stack */

/* the effective priority: real-time policies have 1..99, others 0; a
   priority inherited through a PI futex counts too */
/* real-time throttling, as Linux's sched_rt_runtime_us: real-time tasks
   get at most 950 ms of every second */
static unsigned rt_ticks;

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

int task_prio(struct task *t) {
  if (rt_ticks >= 950 * HZ / 1000)
    return 0;
  int p = (t->policy == 1 || t->policy == 2) ? t->rtprio : 0;
  if (t->policy == SCHED_SPORADIC && t->ss)
    p = t->ss->low ? t->ss->lo : t->ss->hi;
  return t->boost > p ? t->boost : p;
}

void task_prio_changed(struct task *t) {
  if (current && t != current && t->state == T_RUNNING && task_prio(t) > task_prio(current))
    need_resched = true;
  if (t == current)
    need_resched = true; /* a lower priority may let another run */
}

void make_runnable(struct task *t) {
  if (t->run_link.next == &t->run_link && t != idle_task)
    list_add_tail(&runq, &t->run_link);
}

void wake_task(struct task *t) {
  uint64_t f = irq_save();
  if (t->state == T_SLEEPING || t->state == T_UNINTERRUPTIBLE) {
    t->state = T_RUNNING;
    if (t->ss)
      ss_activate(t->ss, monotonic_ns());
    make_runnable(t);
    if (!current || task_prio(t) >= task_prio(current))
      need_resched = true;
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
  if (dead_task && dead_task != current) {
    struct task *t = dead_task;
    dead_task = NULL;
    task_free(t);
  }
}

void schedule_tail(void) {
  finish_switch();
}

void schedule(void) {
  struct task *prev = current;
  need_resched = false;
  if (prev->state == T_RUNNING)
    make_runnable(prev);
  else if (prev->ss)
    ss_stop(prev->ss, monotonic_ns()); /* it blocks */
  /* the first of the highest priority (FIFO among equals) */
  struct task *next = idle_task;
  int best = -1;
  list_for_each(p, &runq) {
    struct task *t = list_entry(p, struct task, run_link);
    int pr = task_prio(t);
    if (pr > best) {
      best = pr;
      next = t;
    }
  }
  if (next != idle_task)
    list_del(&next->run_link);
  if (next == prev)
    return;
  next->slice = SLICE;
  if (!prev->is_kthread && prev->fpu)
    fxsave(prev->fpu);
  current = next;
  if (next->proc && next->proc->mm)
    mm_switch(next->proc->mm);
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

void timer_tick(void) {
  jiffies++;
  if (jiffies % HZ == 0) {
    if (rt_ticks >= 950 * HZ / 1000)
      need_resched = true; /* the throttle ends: real-time priorities count again */
    rt_ticks = 0;
  }
  uint64_t now = monotonic_ns();
  list_for_each_safe(p, n, &timed) {
    struct task *t = list_entry(p, struct task, timer_link);
    if (t->wake_at && now >= t->wake_at) {
      t->timed_out = true;
      wake_task(t);
    }
  }
  struct task *c = current;
  if (c && c != idle_task) {
    /* SCHED_FIFO (and a sporadic server) runs until it blocks or yields,
       but not while real-time tasks are throttled (they are not then) */
    bool throttled = rt_ticks >= 950 * HZ / 1000;
    if (--c->slice <= 0 && (throttled || (c->policy != 1 && c->policy != SCHED_SPORADIC)))
      need_resched = true;
  }
  itimer_tick();
}

/* accounting: called from the timer interrupt with the interrupted frame */
void account_tick(bool user) {
  struct task *c = current;
  if (!list_empty(&ss_list)) {
    uint64_t now = monotonic_ns();
    ss_tick(now);
    if (c && c != idle_task && c->ss)
      ss_charge(c, 1000000000UL / HZ, now);
  }
  if (!c || c == idle_task)
    return;
  if (task_prio(c) > 0 && ++rt_ticks >= 950 * HZ / 1000)
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
}

/* on every return to user mode, with interrupts off */
void return_to_user(struct regs *r) {
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

static long idle_fn(void *arg) {
  (void)arg;
  for (;;) {
    cli();
    if (!list_empty(&runq))
      schedule();
    else
      sti_hlt();
  }
  return 0;
}

static struct task *task_kernel(long (*fn)(void *), void *arg) {
  struct task *t = task_alloc(NULL);
  if (!t)
    return NULL;
  t->is_kthread = true;
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
  list_del(&current->all_link);
  dead_task = current;
  schedule();
  panic("dead kernel thread ran");
}

/* the boot thread becomes the idle task */
void sched_init(void) {
  static uint64_t boot_ksp;
  idle_task = task_alloc(NULL);
  idle_task->tid = 0; /* the idle task is pid 0; init will be 1 */
  reset_pids();
  idle_task->is_kthread = true;
  idle_task->state = T_RUNNING;
  current = idle_task;
  (void)boot_ksp;
}

/* called by the boot thread at the end of kmain: become idle forever */
NORETURN void cpu_idle(void) {
  /* switch onto the idle task's own stack, leaving the boot stack */
  struct task *t = task_kernel(idle_fn, NULL);
  t->tid = 0;
  t->is_kthread = true;
  t->state = T_RUNNING;
  struct task *boot = idle_task;
  idle_task = t;
  current = t;
  set_kernel_stack((uint64_t)t->kstack + (PAGE_SIZE << KSTACK_ORDER));
  boot->state = T_ZOMBIE;
  switch_stack(&boot->ksp, t->ksp);
  panic("boot thread resumed");
}

int kstack_order(void) { return KSTACK_ORDER; }
void reset_pids(void);

void task_set_dead(struct task *t) { dead_task = t; }

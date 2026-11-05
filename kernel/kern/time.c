/* time.c - clocks, sleeping, interval timers and POSIX timers. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "syscall.h"

volatile uint64_t jiffies;
static uint64_t tsc0, tsc_hz;
static int64_t realtime_offset; /* realtime = monotonic + offset */

uint64_t monotonic_ns(void) {
  uint64_t c = rdtsc64() - tsc0;
  return c / tsc_hz * 1000000000UL + c % tsc_hz * 1000000000UL / tsc_hz;
}

uint64_t realtime_ns(void) { return (uint64_t)((int64_t)monotonic_ns() + realtime_offset); }

static void rt_clock_changed(int64_t old_offset);
static struct waitq rt_abs_wait; /* below */
void set_realtime_ns(uint64_t ns) {
  int64_t old = realtime_offset;
  realtime_offset = (int64_t)ns - (int64_t)monotonic_ns();
  rt_clock_changed(old);
}

static unsigned cmos(int reg) {
  outb(0x70, (uint8_t)reg);
  return inb(0x71);
}

static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

static int64_t rtc_read(void) {
  while (cmos(0x0a) & 0x80)
    ;
  unsigned s = cmos(0), mi = cmos(2), h = cmos(4), d = cmos(7), mo = cmos(8), y = cmos(9);
  unsigned b = cmos(0x0b);
  if (!(b & 4)) {
#define BCD(x) (((x) & 15) + ((x) >> 4) * 10)
    s = BCD(s);
    mi = BCD(mi);
    h = BCD(h & 0x7f) | (h & 0x80);
    d = BCD(d);
    mo = BCD(mo);
    y = BCD(y);
  }
  if (!(b & 2) && (h & 0x80))
    h = ((h & 0x7f) + 12) % 24;
  return days_from_civil(2000 + y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

void time_init(void) {
  /* calibrate the TSC with PIT channel 2 over 10 ms */
  outb(0x61, (inb(0x61) & ~0x02) | 0x01);
  outb(0x43, 0xb0);
  unsigned count = 11932;
  outb(0x42, count & 0xff);
  outb(0x42, count >> 8);
  uint64_t t0 = rdtsc64();
  while (!(inb(0x61) & 0x20))
    ;
  uint64_t t1 = rdtsc64();
  tsc_hz = (t1 - t0) * 100;
  if (tsc_hz < 1000000)
    tsc_hz = 1000000;
  tsc0 = rdtsc64();
  set_realtime_ns((uint64_t)rtc_read() * 1000000000UL);
  printk("clock: TSC at %lu kHz\n", tsc_hz / 1000);
}

/* ---- clocks ---- */

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7
#define TIMER_ABSTIME 1

static int clock_ns(int clk, uint64_t *ns) {
  if (clk < 0) {
    /* CPU-time clocks of other processes and threads, Linux's encoding:
       ~id << 3 | (thread ? 4 : 0) | (0 prof, 1 virt, 2 sched) */
    pid_t id = (pid_t)~(clk >> 3);
    int which = clk & 3;
    if (which == 3)
      return -EINVAL;
    uint64_t ticks;
    if (clk & 4) {
      struct task *t = id ? find_task(id) : current;
      if (!t || (id && t->proc != cur_proc()))
        return -EINVAL;
      ticks = which == 1 ? t->utime : t->utime + t->stime;
    } else {
      struct proc *p = id ? find_proc(id) : cur_proc();
      if (!p)
        return -EINVAL;
      ticks = which == 1 ? p->utime : p->utime + p->stime;
    }
    *ns = ticks * (1000000000UL / HZ);
    return 0;
  }
  switch (clk) {
  case CLOCK_REALTIME:
  case CLOCK_REALTIME_COARSE:
    *ns = realtime_ns();
    return 0;
  case CLOCK_MONOTONIC:
  case CLOCK_MONOTONIC_RAW:
  case CLOCK_MONOTONIC_COARSE:
  case CLOCK_BOOTTIME:
    *ns = monotonic_ns();
    return 0;
  case CLOCK_PROCESS_CPUTIME_ID:
    *ns = (cur_proc()->utime + cur_proc()->stime) * (1000000000UL / HZ);
    return 0;
  case CLOCK_THREAD_CPUTIME_ID:
    *ns = (current->utime + current->stime) * (1000000000UL / HZ);
    return 0;
  }
  return -EINVAL;
}

static int put_ts(void *u, uint64_t ns) {
  struct timespec64 ts = {(int64_t)(ns / 1000000000UL), (int64_t)(ns % 1000000000UL)};
  return copy_to_user(u, &ts, sizeof(ts));
}

static int get_ts(const void *u, uint64_t *ns) {
  struct timespec64 ts;
  if (copy_from_user(&ts, u, sizeof(ts)))
    return -EFAULT;
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= 1000000000)
    return -EINVAL;
  *ns = ts_to_ns(ts.sec, ts.nsec);
  return 0;
}

long sys_clock_gettime(long clk, long uts) {
  uint64_t ns;
  int e = clock_ns((int)clk, &ns);
  if (e)
    return e;
  return put_ts((void *)uts, ns) ? -EFAULT : 0;
}

long sys_clock_getres(long clk, long uts) {
  uint64_t ns;
  int e = clock_ns((int)clk, &ns);
  if (e)
    return e;
  if (!uts)
    return 0;
  return put_ts((void *)uts, clk < 0 || clk == CLOCK_PROCESS_CPUTIME_ID || clk == CLOCK_THREAD_CPUTIME_ID ? 1000000000UL / HZ : 1) ? -EFAULT : 0;
}

long sys_clock_settime(long clk, long uts) {
  uint64_t ns;
  if (clk != CLOCK_REALTIME)
    return -EINVAL;
  if (!capable())
    return -EPERM;
  int e = get_ts((void *)uts, &ns);
  if (e)
    return e;
  set_realtime_ns(ns);
  return 0;
}

long sys_gettimeofday(long utv, long utz) {
  if (utv) {
    uint64_t ns = realtime_ns();
    int64_t tv[2] = {(int64_t)(ns / 1000000000UL), (int64_t)(ns % 1000000000UL / 1000)};
    if (copy_to_user((void *)utv, tv, sizeof(tv)))
      return -EFAULT;
  }
  if (utz) {
    int32_t tz[2] = {0, 0};
    if (copy_to_user((void *)utz, tz, sizeof(tz)))
      return -EFAULT;
  }
  return 0;
}

long sys_settimeofday(long utv, long utz) {
  (void)utz;
  if (!capable())
    return -EPERM;
  if (utv) {
    int64_t tv[2];
    if (copy_from_user(tv, (void *)utv, sizeof(tv)))
      return -EFAULT;
    set_realtime_ns((uint64_t)tv[0] * 1000000000UL + (uint64_t)tv[1] * 1000);
  }
  return 0;
}

long sys_time(long ut) {
  int64_t t = (int64_t)(realtime_ns() / 1000000000UL);
  if (ut && copy_to_user((void *)ut, &t, sizeof(t)))
    return -EFAULT;
  return t;
}

/* sleep until the clock reaches abs; remaining time into rem if
   interrupted (relative sleeps only).  Interrupted, it returns
   -ERESTARTNOHAND: a handler makes that EINTR; otherwise (a stop and
   SIGCONT, a signal ignored on delivery) the call is restarted, and a
   relative one goes on to the same deadline (sleep_restart). */
static long do_nanosleep(int clk, uint64_t abs, bool relative, long ureq, void *rem) {
  uint64_t f = irq_save();
  long r = 0;
  for (;;) {
    uint64_t now;
    clock_ns(clk, &now);
    if (now >= abs)
      break;
    uint64_t deadline = sat_add(monotonic_ns(), abs - now);
    int e = sleep_on(!relative && clk == CLOCK_REALTIME ? &rt_abs_wait : NULL, deadline, true);
    if (e == -EINTR) {
      r = -ERESTARTNOHAND;
      break;
    }
  }
  irq_restore(f);
  if (r && relative) {
    current->sleep_restart = true;
    current->sleep_restart_clk = clk;
    current->sleep_restart_req = ureq;
    current->sleep_restart_abs = abs;
    if (rem) {
      uint64_t now;
      clock_ns(clk, &now);
      if (put_ts(rem, abs > now ? abs - now : 0))
        return -EFAULT;
    }
  }
  return r;
}

/* the deadline of a relative sleep of ns: a restarted one keeps its own */
static uint64_t rel_deadline(int clk, long ureq, uint64_t now, uint64_t ns) {
  if (current->sleep_restart && current->sleep_restart_clk == clk && current->sleep_restart_req == ureq) {
    current->sleep_restart = false;
    return current->sleep_restart_abs;
  }
  current->sleep_restart = false;
  return sat_add(now, ns);
}

long sys_nanosleep(long ureq, long urem) {
  uint64_t ns;
  int e = get_ts((void *)ureq, &ns);
  if (e)
    return e;
  uint64_t abs = rel_deadline(CLOCK_MONOTONIC, ureq, monotonic_ns(), ns);
  return do_nanosleep(CLOCK_MONOTONIC, abs, true, ureq, (void *)urem);
}

long sys_clock_nanosleep(long clk, long flags, long ureq, long urem) {
  uint64_t ns, now;
  if (clk == CLOCK_THREAD_CPUTIME_ID)
    return -EINVAL;
  int e = clock_ns((int)clk, &now);
  if (e)
    return e;
  e = get_ts((void *)ureq, &ns);
  if (e)
    return e;
  if (flags & TIMER_ABSTIME)
    return do_nanosleep((int)clk, ns, false, ureq, NULL);
  /* a relative sleep on CLOCK_REALTIME is not moved by clock_settime
     (POSIX): it is measured on the monotonic clock */
  if (clk == CLOCK_REALTIME) {
    clk = CLOCK_MONOTONIC;
    now = monotonic_ns();
  }
  return do_nanosleep((int)clk, rel_deadline((int)clk, ureq, now, ns), true, ureq, (void *)urem);
}

long sys_times(long ubuf) {
  struct proc *p = cur_proc();
  /* clock_t in USER_HZ (100) */
  int64_t t[4] = {(int64_t)(p->utime * 100 / HZ), (int64_t)(p->stime * 100 / HZ),
                  (int64_t)(p->cutime * 100 / HZ), (int64_t)(p->cstime * 100 / HZ)};
  if (ubuf && copy_to_user((void *)ubuf, t, sizeof(t)))
    return -EFAULT;
  return (long)(monotonic_ns() / 10000000UL);
}

/* ---- interval timers and POSIX timers ---- */

struct ktimer {
  struct list link;  /* proc->timers */
  struct list alink; /* armed timers */
  struct proc *proc;
  int id;
  int clock;
  int signo, notify;
  pid_t tid;
  uint64_t value;
  uint64_t expires, interval; /* monotonic ns; expires 0 = disarmed */
  int overrun, pending_overrun;
  bool queued;
  bool abs_rt; /* armed with TIMER_ABSTIME on CLOCK_REALTIME: follows that clock */
};

#define SIGEV_SIGNAL 0
#define SIGEV_NONE 1
#define SIGEV_THREAD 2
#define SIGEV_THREAD_ID 4

static struct list armed = LIST_INIT(armed);
/* absolute sleeps on CLOCK_REALTIME, woken to look again when it is set */
static struct waitq rt_abs_wait = WAITQ_INIT(rt_abs_wait);

/* CLOCK_REALTIME was set (POSIX: absolute timers and sleeps on it expire
 * as the new time says; relative ones are not affected) */
static void rt_clock_changed(int64_t old_offset) {
  uint64_t f = irq_save();
  uint64_t now = monotonic_ns();
  list_for_each(a, &armed) {
    struct ktimer *t = list_entry(a, struct ktimer, alink);
    if (!t->abs_rt)
      continue;
    int64_t rt = (int64_t)t->expires + old_offset; /* the time of day it was set for */
    int64_t mono = rt - realtime_offset;
    t->expires = mono <= (int64_t)now ? now : (uint64_t)mono;
  }
  wake_up(&rt_abs_wait);
  irq_restore(f);
}

#define DELAYTIMER_MAX 0x7fffffff
static int sat_overrun(int a, int b) {
  return a > DELAYTIMER_MAX - b ? DELAYTIMER_MAX : a + b;
}

/* POSIX: one signal of a timer is queued at a time; an expiration while
 * it is still pending is an overrun, counted until the signal is taken,
 * when timer_getoverrun() gets the count (timer_signal_taken) */
static void timer_fire(struct ktimer *t, int missed) {
  if (t->notify == SIGEV_NONE || !t->signo)
    return;
  if (t->queued) {
    t->overrun = sat_overrun(t->overrun, missed + 1);
    return;
  }
  struct siginfo si;
  memset(&si, 0, sizeof(si));
  si.signo = t->signo;
  si.code = SI_TIMER;
  si.u.timer.tid = t->id;
  si.u.timer.value = t->value;
  t->overrun = missed;
  struct task *tt = (t->notify & SIGEV_THREAD_ID) ? find_task(t->tid) : NULL;
  if (tt && tt->proc == t->proc)
    send_signal_task(tt, &si);
  else
    send_signal_proc(t->proc, &si);
  if (signal_was_queued())
    t->queued = true;
  else {
    /* discarded (ignored), or the signal was pending from elsewhere */
    t->pending_overrun = t->overrun;
    t->overrun = 0;
  }
}

void timer_signal_taken(struct proc *p, struct siginfo *si) {
  list_for_each(l, &p->timers) {
    struct ktimer *t = list_entry(l, struct ktimer, link);
    if (t->id == si->u.timer.tid) {
      if (t->queued) {
        t->pending_overrun = t->overrun;
        si->u.timer.overrun = t->overrun;
        t->overrun = 0;
        t->queued = false;
      }
      return;
    }
  }
}

void itimer_tick(void) {
  uint64_t now = monotonic_ns();
  /* alarm / ITIMER_REAL of every process */
  list_for_each(pp, &all_procs) {
    struct proc *p = list_entry(pp, struct proc, all_link);
    struct itimer *it = &p->itimer[0];
    if (p->state == P_ALIVE && it->expires && now >= it->expires) {
      if (it->interval) {
        while (it->expires <= now)
          it->expires += it->interval;
      } else
        it->expires = 0;
      signal_simple(p, SIGALRM);
    }
  }
  list_for_each_safe(a, n, &armed) {
    struct ktimer *t = list_entry(a, struct ktimer, alink);
    if (now < t->expires)
      continue;
    int missed = 0;
    if (t->interval) {
      uint64_t n = (now - t->expires) / t->interval;  /* expirations passed besides this one */
      t->expires += (n + 1) * t->interval;
      missed = n > DELAYTIMER_MAX ? DELAYTIMER_MAX : (int)n;
    } else {
      t->expires = 0;
      list_del(&t->alink);
    }
    timer_fire(t, missed);
  }
}

long sys_alarm(long secs) {
  struct itimer *it = &cur_proc()->itimer[0];
  uint64_t now = monotonic_ns();
  long left = 0;
  if (it->expires) {
    uint64_t r = it->expires > now ? it->expires - now : 0;
    left = (long)((r + 999999999UL) / 1000000000UL);
    if (left == 0)
      left = 1;
  }
  it->interval = 0;
  it->expires = secs ? sat_add(now, ts_to_ns((unsigned)secs, 0)) : 0;
  return left;
}

struct kitimerval {
  int64_t isec, iusec, vsec, vusec;
};

static void itv_get(struct itimer *it, struct kitimerval *v) {
  uint64_t now = monotonic_ns();
  uint64_t r = it->expires > now ? it->expires - now : (it->expires ? 1000 : 0);
  v->isec = (int64_t)(it->interval / 1000000000UL);
  v->iusec = (int64_t)(it->interval % 1000000000UL / 1000);
  v->vsec = (int64_t)(r / 1000000000UL);
  v->vusec = (int64_t)(r % 1000000000UL / 1000);
}

long sys_getitimer(long which, long uv) {
  if (which < 0 || which > 2)
    return -EINVAL;
  struct kitimerval v;
  itv_get(&cur_proc()->itimer[which], &v);
  return copy_to_user((void *)uv, &v, sizeof(v)) ? -EFAULT : 0;
}

long sys_setitimer(long which, long unew, long uold) {
  if (which < 0 || which > 2)
    return -EINVAL;
  struct itimer *it = &cur_proc()->itimer[which];
  struct kitimerval v, o;
  if (unew && copy_from_user(&v, (void *)unew, sizeof(v)))
    return -EFAULT;
  itv_get(it, &o);
  if (unew) {
    if (v.iusec < 0 || v.iusec >= 1000000 || v.vusec < 0 || v.vusec >= 1000000)
      return -EINVAL;
    uint64_t f = irq_save();
    it->interval = (uint64_t)v.isec * 1000000000UL + (uint64_t)v.iusec * 1000;
    uint64_t val = sat_add(ts_to_ns(v.vsec, 0), (uint64_t)v.vusec * 1000);
    it->expires = val ? sat_add(monotonic_ns(), val) : 0;
    irq_restore(f);
  }
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  return 0;
}

struct ksigevent {
  uint64_t value;
  int32_t signo, notify;
  int32_t tid;
  int32_t pad[11];
};

static struct ktimer *timer_find(int id) {
  list_for_each(p, &cur_proc()->timers) {
    struct ktimer *t = list_entry(p, struct ktimer, link);
    if (t->id == id)
      return t;
  }
  return NULL;
}

long sys_timer_create(long clk, long usev, long uid) {
  uint64_t dummy;
  if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC && clk != CLOCK_BOOTTIME &&
      clock_ns((int)clk, &dummy))
    return -EINVAL;
  struct ksigevent ev;
  memset(&ev, 0, sizeof(ev));
  struct proc *p = cur_proc();
  if (usev) {
    if (copy_from_user(&ev, (void *)usev, sizeof(ev)))
      return -EFAULT;
  } else {
    ev.signo = SIGALRM;
    ev.notify = SIGEV_SIGNAL;
  }
  if (ev.notify != SIGEV_NONE && (ev.signo < 0 || ev.signo > NSIG))
    return -EINVAL;
  if ((ev.notify & SIGEV_THREAD_ID)) {
    struct task *t = find_task(ev.tid);
    if (!t || t->proc != p)
      return -EINVAL;
  }
  struct ktimer *t = kzalloc(sizeof(*t));
  if (!t)
    return -EAGAIN;
  t->proc = p;
  t->id = p->next_timer_id++;
  t->clock = (int)clk;
  t->signo = ev.signo;
  t->notify = ev.notify;
  t->tid = ev.tid;
  t->value = usev ? ev.value : (uint64_t)t->id;
  list_init(&t->alink);
  int32_t id = t->id;
  if (copy_to_user((void *)uid, &id, sizeof(id))) {
    kfree(t);
    return -EFAULT;
  }
  list_add_tail(&p->timers, &t->link);
  return 0;
}

struct kitimerspec {
  int64_t isec, insec, vsec, vnsec;
};

static void its_get(struct ktimer *t, struct kitimerspec *v) {
  uint64_t now = monotonic_ns();
  uint64_t r = t->expires > now ? t->expires - now : (t->expires ? 1 : 0);
  v->isec = (int64_t)(t->interval / 1000000000UL);
  v->insec = (int64_t)(t->interval % 1000000000UL);
  v->vsec = (int64_t)(r / 1000000000UL);
  v->vnsec = (int64_t)(r % 1000000000UL);
}

long sys_timer_settime(long id, long flags, long unew, long uold) {
  struct ktimer *t = timer_find((int)id);
  if (!t)
    return -EINVAL;
  struct kitimerspec v, o;
  if (copy_from_user(&v, (void *)unew, sizeof(v)))
    return -EFAULT;
  if (v.insec < 0 || v.insec >= 1000000000 || v.vnsec < 0 || v.vnsec >= 1000000000 ||
      v.isec < 0 || v.vsec < 0)
    return -EINVAL;
  its_get(t, &o);
  uint64_t f = irq_save();
  if (t->expires)
    list_del(&t->alink);
  t->expires = 0;
  t->overrun = 0;
  t->abs_rt = (flags & TIMER_ABSTIME) && t->clock == CLOCK_REALTIME;
  t->interval = (uint64_t)v.isec * 1000000000UL + (uint64_t)v.insec;
  uint64_t val = ts_to_ns(v.vsec, v.vnsec);
  if (val) {
    uint64_t now;
    clock_ns(t->clock, &now);
    uint64_t rel = (flags & TIMER_ABSTIME) ? (val > now ? val - now : 0) : val;
    t->expires = sat_add(monotonic_ns(), rel);
    if (rel == 0)
      t->expires = monotonic_ns();
    list_add_tail(&armed, &t->alink);
  }
  irq_restore(f);
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  return 0;
}

long sys_timer_gettime(long id, long uv) {
  struct ktimer *t = timer_find((int)id);
  if (!t)
    return -EINVAL;
  struct kitimerspec v;
  its_get(t, &v);
  return copy_to_user((void *)uv, &v, sizeof(v)) ? -EFAULT : 0;
}

long sys_timer_getoverrun(long id) {
  struct ktimer *t = timer_find((int)id);
  if (!t)
    return -EINVAL;
  return t->pending_overrun;
}

static void timer_free(struct ktimer *t) {
  uint64_t f = irq_save();
  if (t->expires)
    list_del(&t->alink);
  list_del(&t->link);
  irq_restore(f);
  kfree(t);
}

long sys_timer_delete(long id) {
  struct ktimer *t = timer_find((int)id);
  if (!t)
    return -EINVAL;
  timer_free(t);
  return 0;
}

void timers_exit_posix(struct proc *p) {
  list_for_each_safe(a, n, &p->timers) timer_free(list_entry(a, struct ktimer, link));
}

void timers_exit(struct proc *p) {
  list_for_each_safe(a, n, &p->timers) timer_free(list_entry(a, struct ktimer, link));
  memset(p->itimer, 0, sizeof(p->itimer));
}

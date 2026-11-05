/* futex.c - fast user-space locking: wait/wake/requeue/wake_op, the
   priority-inheritance protocol (ownership hand-over only: one processor and
   no priorities to inherit), and robust lists at thread exit.  The kernel is
   not preemptive and runs on one processor, so reading and writing a futex
   word here is atomic with respect to user threads. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "syscall.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_LOCK_PI2 13
#define FUTEX_PRIVATE 128
#define FUTEX_CLOCK_REALTIME 256

#define FUTEX_WAITERS 0x80000000U
#define FUTEX_OWNER_DIED 0x40000000U
#define FUTEX_TID_MASK 0x3fffffffU

static struct list waiters = LIST_INIT(waiters);
/* waiters of a real-time priority (task_prio > 0 when they queued): while
   there are none, a wake-up takes the first waiters, without a search */
static int rt_waiters;

static void enqueue(struct task *t) {
  t->fprio = task_prio(t);
  if (t->fprio)
    rt_waiters++;
  list_add_tail(&waiters, &t->futex_link);
}

static void unqueue(struct task *t) {
  list_del(&t->futex_link);
  if (t->fprio)
    rt_waiters--;
  t->fprio = 0;
}

static int get_key(uint64_t uaddr, bool priv, uint64_t *k1, uint64_t *k2) {
  if (uaddr & 3)
    return -EINVAL;
  struct mm *mm = cur_proc()->mm;
  if (priv) {
    *k1 = (uint64_t)mm;
    *k2 = uaddr;
    return 0;
  }
  if (!user_range_ok(uaddr, 4))
    return -EFAULT;
  uint64_t *pte = pte_get(mm->pml4, uaddr, false);
  if (!pte || !(*pte & PTE_P)) {
    if (handle_fault(mm, uaddr, false, false))
      return -EFAULT;
    pte = pte_get(mm->pml4, uaddr, false);
  }
  if (!pte || !(*pte & PTE_P))
    return -EFAULT;
  struct vma *v = vma_find(mm, uaddr);
  if (v && !(v->flags & VM_SHARED)) {
    /* private memory: the address space identifies it */
    *k1 = (uint64_t)mm;
    *k2 = uaddr;
    return 0;
  }
  *k1 = 0;
  *k2 = (*pte & PTE_ADDR) | (uaddr & 0xfff);
  return 0;
}

static int read_word(uint64_t uaddr, uint32_t *v) { return copy_from_user(v, (void *)uaddr, 4); }

static int write_word(uint64_t uaddr, uint32_t v) {
  if (copy_to_user((void *)uaddr, &v, 4))
    return -EFAULT;
  return 0;
}

/* the waiters woken are those of the highest priority, the longest waiting
   first among equals (as Linux: so sem_post, pthread_cond_signal and the
   unlocks of the C library wake as POSIX's scheduling options require) */
static int wake(uint64_t k1, uint64_t k2, int n, uint32_t bitset) {
  int woken = 0;
  uint64_t f = irq_save();
  if (n >= 0x7fffffff || !rt_waiters) { /* all of them, or none real-time: in order */
    list_for_each_safe(p, nx, &waiters) {
      struct task *t = list_entry(p, struct task, futex_link);
      if (woken >= n)
        break;
      if (t->fkey1 != k1 || t->fkey2 != k2 || !(t->fbitset & bitset))
        continue;
      unqueue(t);
      t->fwoken = true;
      wake_task(t);
      woken++;
    }
    n = 0;
  }
  while (woken < n) {
    struct task *best = NULL;
    int bp = -1;
    list_for_each(p, &waiters) {
      struct task *t = list_entry(p, struct task, futex_link);
      if (t->fkey1 != k1 || t->fkey2 != k2 || !(t->fbitset & bitset))
        continue;
      int pr = task_prio(t);
      if (pr > bp) {
        best = t;
        bp = pr;
      }
    }
    if (!best)
      break;
    unqueue(best);
    best->fwoken = true;
    wake_task(best);
    woken++;
  }
  irq_restore(f);
  return woken;
}

/* wake one PI waiter so that it tries again (the owner died) */
static void wake_retry(uint64_t uaddr) {
  uint64_t k[2][2] = {{0, 0}, {(uint64_t)cur_proc()->mm, uaddr}};
  if (get_key(uaddr, false, &k[0][0], &k[0][1]))
    k[0][0] = k[0][1] = 1;
  list_for_each(p, &waiters) {
    struct task *t = list_entry(p, struct task, futex_link);
    for (int i = 0; i < 2; i++)
      if (t->fkey1 == k[i][0] && t->fkey2 == k[i][1]) {
        wake_task(t);
        return;
      }
  }
}

void futex_wake_addr(uint64_t uaddr, int n) {
  uint64_t k1, k2;
  if (get_key(uaddr, false, &k1, &k2) == 0)
    wake(k1, k2, n, ~0U);
  /* a private waiter keyed by address space */
  wake((uint64_t)cur_proc()->mm, uaddr, n, ~0U);
}

static uint64_t abs_deadline(uint64_t uts, bool absolute, bool realtime, int *err) {
  *err = 0;
  if (!uts)
    return 0;
  struct timespec64 ts;
  if (copy_from_user(&ts, (void *)uts, sizeof(ts))) {
    *err = -EFAULT;
    return 0;
  }
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= 1000000000) {
    *err = -EINVAL;
    return 0;
  }
  uint64_t ns = ts_to_ns(ts.sec, ts.nsec);
  uint64_t now = monotonic_ns();
  if (!absolute)
    return sat_add(now, ns + (ns == 0));
  if (realtime) {
    uint64_t rt = realtime_ns();
    return ns > rt ? sat_add(now, ns - rt) : now ? now : 1;
  }
  return ns ? ns : 1;
}

/* wait while *uaddr == val */
static long futex_wait(uint64_t uaddr, bool priv, uint32_t val, uint64_t deadline, uint32_t bitset) {
  if (!bitset)
    return -EINVAL;
  uint64_t k1, k2;
  int e = get_key(uaddr, priv, &k1, &k2);
  if (e)
    return e;
  uint64_t f = irq_save();
  uint32_t cur;
  if (read_word(uaddr, &cur)) {
    irq_restore(f);
    return -EFAULT;
  }
  if (cur != val) {
    irq_restore(f);
    return -EAGAIN;
  }
  struct task *t = current;
  t->fkey1 = k1;
  t->fkey2 = k2;
  t->fbitset = bitset;
  t->fwoken = false;
  enqueue(t);
  int r = 0;
  while (!t->fwoken) {
    r = sleep_on(NULL, deadline, true);
    if (r)
      break;
  }
  if (!t->fwoken)
    unqueue(t);
  irq_restore(f);
  if (t->fwoken)
    return 0;
  if (r == -ETIMEDOUT)
    return -ETIMEDOUT;
  return deadline ? -EINTR : -ERESTARTSYS;
}

static long requeue(uint64_t u1, uint64_t u2, bool priv, int nwake, int nreq, bool cmp, uint32_t val) {
  uint64_t a1, a2, b1, b2;
  int e = get_key(u1, priv, &a1, &a2);
  if (e)
    return e;
  e = get_key(u2, priv, &b1, &b2);
  if (e)
    return e;
  uint64_t f = irq_save();
  if (cmp) {
    uint32_t cur;
    if (read_word(u1, &cur)) {
      irq_restore(f);
      return -EFAULT;
    }
    if (cur != val) {
      irq_restore(f);
      return -EAGAIN;
    }
  }
  int woken = wake(a1, a2, nwake, ~0U);
  int moved = 0;
  list_for_each(p, &waiters) {
    if (moved >= nreq)
      break;
    struct task *t = list_entry(p, struct task, futex_link);
    if (t->fkey1 == a1 && t->fkey2 == a2) {
      t->fkey1 = b1;
      t->fkey2 = b2;
      moved++;
    }
  }
  irq_restore(f);
  return woken + moved;
}

static long wake_op(uint64_t u1, uint64_t u2, bool priv, int n1, int n2, uint32_t op) {
  uint64_t a1, a2, b1, b2;
  int e = get_key(u1, priv, &a1, &a2);
  if (e)
    return e;
  e = get_key(u2, priv, &b1, &b2);
  if (e)
    return e;
  int opc = (op >> 28) & 7, cmp = (op >> 24) & 15;
  uint32_t oparg = (op >> 12) & 0xfff, cmparg = op & 0xfff;
  if (op & 0x80000000U)
    oparg = 1U << (oparg & 31);
  uint64_t f = irq_save();
  uint32_t old, nv;
  if (read_word(u2, &old)) {
    irq_restore(f);
    return -EFAULT;
  }
  switch (opc) {
  case 0: nv = oparg; break;
  case 1: nv = old + oparg; break;
  case 2: nv = old | oparg; break;
  case 3: nv = old & ~oparg; break;
  case 4: nv = old ^ oparg; break;
  default:
    irq_restore(f);
    return -ENOSYS;
  }
  if (write_word(u2, nv)) {
    irq_restore(f);
    return -EFAULT;
  }
  int r = wake(a1, a2, n1, ~0U);
  bool c;
  int32_t so = (int32_t)old, sc = (int32_t)cmparg;
  switch (cmp) {
  case 0: c = so == sc; break;
  case 1: c = so != sc; break;
  case 2: c = so < sc; break;
  case 3: c = so <= sc; break;
  case 4: c = so > sc; break;
  case 5: c = so >= sc; break;
  default: c = false;
  }
  if (c)
    r += wake(b1, b2, n2, ~0U);
  irq_restore(f);
  return r;
}

/* ---- priority inheritance protocol ---- */

static bool tid_alive(uint32_t tid) {
  struct task *t = find_task((pid_t)tid);
  return t && !t->exiting;
}

static long lock_pi(uint64_t uaddr, bool priv, uint64_t deadline, bool try) {
  uint64_t k1, k2;
  int e = get_key(uaddr, priv, &k1, &k2);
  if (e)
    return e;
  uint32_t tid = (uint32_t)current->tid;
  uint64_t f = irq_save();
  for (;;) {
    uint32_t w;
    if (read_word(uaddr, &w)) {
      irq_restore(f);
      return -EFAULT;
    }
    uint32_t owner = w & FUTEX_TID_MASK;
    if (owner == tid) {
      irq_restore(f);
      return -EDEADLK;
    }
    if (owner == 0 || !tid_alive(owner)) {
      /* free, or the owner died: take it (keep the waiters bit) */
      uint32_t nw = tid | (w & FUTEX_WAITERS);
      if (owner != 0)
        nw |= FUTEX_OWNER_DIED;
      else if (w & FUTEX_OWNER_DIED)
        nw |= FUTEX_OWNER_DIED;
      if (write_word(uaddr, nw)) {
        irq_restore(f);
        return -EFAULT;
      }
      irq_restore(f);
      return 0;
    }
    if (try) {
      irq_restore(f);
      return -EBUSY;
    }
    /* priority inheritance: the owner runs at least at our priority */
    struct task *o = find_task((pid_t)owner);
    if (o && task_prio(current) > task_prio(o)) {
      o->boost = task_prio(current);
      task_prio_changed(o);
    }
    if (!(w & FUTEX_WAITERS) && write_word(uaddr, w | FUTEX_WAITERS)) {
      irq_restore(f);
      return -EFAULT;
    }
    struct task *t = current;
    t->fkey1 = k1;
    t->fkey2 = k2;
    t->fbitset = ~0U;
    t->fwoken = false;
    enqueue(t);
    int r = sleep_on(NULL, deadline, true);
    if (t->fwoken) {
      /* the unlocker made us the owner */
      irq_restore(f);
      return 0;
    }
    unqueue(t);
    if (r == -ETIMEDOUT) {
      irq_restore(f);
      return -ETIMEDOUT;
    }
    if (r == -EINTR) {
      irq_restore(f);
      return deadline ? -EINTR : -ERESTARTNOINTR;
    }
    /* woken without the lock (its owner died): try again */
  }
}

static long unlock_pi(uint64_t uaddr, bool priv) {
  uint64_t k1, k2;
  int e = get_key(uaddr, priv, &k1, &k2);
  if (e)
    return e;
  uint32_t tid = (uint32_t)current->tid;
  uint64_t f = irq_save();
  uint32_t w;
  if (read_word(uaddr, &w)) {
    irq_restore(f);
    return -EFAULT;
  }
  if ((w & FUTEX_TID_MASK) != tid) {
    irq_restore(f);
    return -EPERM;
  }
  /* hand the lock to the first waiter */
  struct task *next = NULL;
  int count = 0;
  list_for_each(p, &waiters) {
    struct task *t = list_entry(p, struct task, futex_link);
    if (t->fkey1 == k1 && t->fkey2 == k2) {
      if (!next)
        next = t;
      count++;
    }
  }
  uint32_t nw = 0;
  if (next) {
    nw = (uint32_t)next->tid | (count > 1 ? FUTEX_WAITERS : 0);
    unqueue(next);
    next->fwoken = true;
    wake_task(next);
  }
  e = write_word(uaddr, nw);
  /* the inherited priority goes; the new owner inherits from the rest */
  if (current->boost) {
    current->boost = 0;
    task_prio_changed(current);
  }
  if (next && count > 1) {
    int maxp = 0;
    list_for_each(p, &waiters) {
      struct task *t = list_entry(p, struct task, futex_link);
      if (t->fkey1 == k1 && t->fkey2 == k2 && task_prio(t) > maxp)
        maxp = task_prio(t);
    }
    if (maxp > task_prio(next))
      next->boost = maxp;
  }
  irq_restore(f);
  return e;
}

long sys_futex(long uaddr, long op, long val, long uts, long uaddr2, long val3) {
  int cmd = (int)op & 127;
  bool priv = op & FUTEX_PRIVATE;
  bool rt = op & FUTEX_CLOCK_REALTIME;
  int err;
  uint64_t deadline;
  switch (cmd) {
  case FUTEX_WAIT:
    deadline = abs_deadline((uint64_t)uts, false, false, &err);
    if (err)
      return err;
    return futex_wait((uint64_t)uaddr, priv, (uint32_t)val, deadline, ~0U);
  case FUTEX_WAIT_BITSET:
    deadline = abs_deadline((uint64_t)uts, true, rt, &err);
    if (err)
      return err;
    return futex_wait((uint64_t)uaddr, priv, (uint32_t)val, deadline, (uint32_t)val3);
  case FUTEX_WAKE:
  case FUTEX_WAKE_BITSET: {
    uint64_t k1, k2;
    int e = get_key((uint64_t)uaddr, priv, &k1, &k2);
    if (e)
      return e;
    return wake(k1, k2, (int)val, cmd == FUTEX_WAKE ? ~0U : (uint32_t)val3);
  }
  case FUTEX_REQUEUE:
    return requeue((uint64_t)uaddr, (uint64_t)uaddr2, priv, (int)val, (int)uts, false, 0);
  case FUTEX_CMP_REQUEUE:
    return requeue((uint64_t)uaddr, (uint64_t)uaddr2, priv, (int)val, (int)uts, true, (uint32_t)val3);
  case FUTEX_WAKE_OP:
    return wake_op((uint64_t)uaddr, (uint64_t)uaddr2, priv, (int)val, (int)uts, (uint32_t)val3);
  case FUTEX_LOCK_PI:
    deadline = abs_deadline((uint64_t)uts, true, true, &err);
    if (err)
      return err;
    return lock_pi((uint64_t)uaddr, priv, deadline, false);
  case FUTEX_LOCK_PI2:
    deadline = abs_deadline((uint64_t)uts, true, rt, &err);
    if (err)
      return err;
    return lock_pi((uint64_t)uaddr, priv, deadline, false);
  case FUTEX_TRYLOCK_PI:
    return lock_pi((uint64_t)uaddr, priv, 0, true);
  case FUTEX_UNLOCK_PI:
    return unlock_pi((uint64_t)uaddr, priv);
  }
  return -ENOSYS;
}

/* robust list: struct robust_list_head { next; long futex_offset; pending } */
static void robust_one(uint64_t entry, int64_t off, bool pi) {
  uint64_t uaddr = entry + (uint64_t)off;
  uint32_t w;
  if (read_word(uaddr, &w))
    return;
  if ((w & FUTEX_TID_MASK) != (uint32_t)current->tid)
    return;
  uint32_t nw = (w & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
  if (write_word(uaddr, nw))
    return;
  if (w & FUTEX_WAITERS) {
    if (pi)
      wake_retry(uaddr);
    else
      futex_wake_addr(uaddr, 1);
  }
}

void exit_robust_list(struct task *t) {
  uint64_t head = t->robust_list;
  if (!head || !t->proc || !t->proc->mm)
    return;
  uint64_t h[3];
  if (copy_from_user(h, (void *)head, sizeof(h)))
    return;
  int64_t off = (int64_t)h[1];
  uint64_t e = h[0];
  for (int n = 0; n < 2048 && e != head; n++) {
    uint64_t next;
    if (copy_from_user(&next, (void *)(e & ~1UL), 8))
      break;
    if ((e & ~1UL) != (h[2] & ~1UL))
      robust_one(e & ~1UL, off, e & 1);
    e = next;
  }
  if (h[2])
    robust_one(h[2] & ~1UL, off, h[2] & 1);
}

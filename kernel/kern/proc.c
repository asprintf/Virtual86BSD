/* proc.c - tasks and processes: creation (clone), exit, wait, process
   groups and sessions, credentials, limits and other process attributes. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "tty.h"
#include "syscall.h"

struct list all_tasks = LIST_INIT(all_tasks);
struct list all_procs = LIST_INIT(all_procs);
struct proc *init_proc;
static _Alignas(16) uint8_t clean_fpu[512];
static pid_t last_pid;

int kstack_order(void);
void task_set_dead(struct task *t);

#define PID_MAX 32768

struct task *find_task(pid_t tid) {
  list_for_each(p, &all_tasks) {
    struct task *t = list_entry(p, struct task, all_link);
    if (t->tid == tid)
      return t;
  }
  return NULL;
}

struct proc *find_proc(pid_t pid) {
  list_for_each(p, &all_procs) {
    struct proc *pr = list_entry(p, struct proc, all_link);
    if (pr->pid == pid && pr->state != P_DEAD)
      return pr;
  }
  return NULL;
}

static bool pid_used(pid_t pid) {
  if (find_task(pid))
    return true;
  list_for_each(p, &all_procs) {
    struct proc *pr = list_entry(p, struct proc, all_link);
    if (pr->pid == pid || pr->pgid == pid || pr->sid == pid)
      return true;
  }
  return false;
}

void reset_pids(void) { last_pid = 0; }

pid_t alloc_pid(void) {
  for (int i = 0; i < PID_MAX; i++) {
    last_pid = last_pid >= PID_MAX - 1 ? 300 : last_pid + 1;
    if (last_pid == 1 && init_proc)
      continue;
    if (!pid_used(last_pid))
      return last_pid;
  }
  return -1;
}

struct task *task_alloc(struct proc *p) {
  struct task *t = kzalloc(sizeof(*t));
  if (!t)
    return NULL;
  uint64_t ks = alloc_pages(kstack_order());
  t->fpu = kmalloc(512);
  if (!ks || !t->fpu) {
    if (ks)
      free_pages(ks, kstack_order());
    kfree(t->fpu);
    kfree(t);
    return NULL;
  }
  t->kstack = P2V(ks);
  memcpy(t->fpu, clean_fpu, 512);
  t->regs = (struct regs *)((char *)t->kstack + (PAGE_SIZE << kstack_order()) - sizeof(struct regs));
  t->proc = p;
  t->tid = alloc_pid();
  list_init(&t->thread_link);
  list_init(&t->run_link);
  list_init(&t->wait_link);
  list_init(&t->timer_link);
  list_init(&t->futex_link);
  list_init(&t->pending.queue);
  t->altstack.flags = SS_DISABLE;
  list_add_tail(&all_tasks, &t->all_link);
  return t;
}

static void pending_clear(struct sigpending *sp) {
  list_for_each_safe(p, n, &sp->queue) {
    list_del(p);
    kfree(list_entry(p, struct sigqueue, link));
  }
  sp->set = 0;
}

void task_free(struct task *t) {
  ss_free(t);
  pending_clear(&t->pending);
  free_pages(V2P(t->kstack), kstack_order());
  kfree(t->fpu);
  kfree(t);
}

struct proc *proc_alloc(void) {
  struct proc *p = kzalloc(sizeof(*p));
  if (!p)
    return NULL;
  list_init(&p->children);
  list_init(&p->sibling);
  list_init(&p->threads);
  list_init(&p->pending.queue);
  list_init(&p->timers);
  waitq_init(&p->child_wait);
  waitq_init(&p->vfork_wait);
  p->state = P_ALIVE;
  p->start_time = monotonic_ns();
  return p;
}

bool capable(void) { return cur_proc()->cred.euid == 0; }

/* the first process (pid 1): made by the kernel, it execs /sbin/init */
void proc_init_first(void) {
  fxsave(clean_fpu); /* the kernel never uses the FPU: a clean state */
  struct proc *p = proc_alloc();
  struct task *t = task_alloc(p);
  p->pid = t->tid;
  p->pgid = p->sid = p->pid;
  p->parent = p;
  p->umask = 022;
  p->mm = mm_new();
  p->nice = 0;
  p->next_timer_id = 0;
  for (int i = 0; i < RLIM_NLIMITS; i++)
    p->rlim[i].cur = p->rlim[i].max = RLIM_INFINITY;
  p->rlim[RLIMIT_STACK].cur = 8UL << 20;
  p->rlim[RLIMIT_NOFILE].cur = 1024;
  p->rlim[RLIMIT_NOFILE].max = 1024 * 1024;
  p->rlim[RLIMIT_CORE].cur = 0;
  p->rlim[RLIMIT_MEMLOCK].cur = p->rlim[RLIMIT_MEMLOCK].max = 8UL << 20;
  p->rlim[RLIMIT_SIGPENDING].cur = p->rlim[RLIMIT_SIGPENDING].max = 1024;
  p->rlim[RLIMIT_NPROC].cur = p->rlim[RLIMIT_NPROC].max = 4096;
  p->rlim[RLIMIT_MSGQUEUE].cur = p->rlim[RLIMIT_MSGQUEUE].max = 819200;
  p->rlim[RLIMIT_NICE].cur = p->rlim[RLIMIT_NICE].max = 0;
  p->rlim[RLIMIT_RTPRIO].cur = p->rlim[RLIMIT_RTPRIO].max = 0;
  strcpy(p->comm, "init");
  p->cwd.mnt = p->root.mnt = root_mount;
  p->cwd.ino = igrab(root_mount->root);
  p->root.ino = igrab(root_mount->root);
  list_add_tail(&p->threads, &t->thread_link);
  p->nthreads = 1;
  list_add_tail(&all_procs, &p->all_link);
  init_proc = p;
  t->state = T_RUNNING;
}

/* ---- clone ---- */

#define CLONE_VM 0x100
#define CLONE_FS 0x200
#define CLONE_FILES 0x400
#define CLONE_SIGHAND 0x800
#define CLONE_PIDFD 0x1000
#define CLONE_VFORK 0x4000
#define CLONE_PARENT 0x8000
#define CLONE_THREAD 0x10000
#define CLONE_SETTLS 0x80000
#define CLONE_PARENT_SETTID 0x100000
#define CLONE_CHILD_CLEARTID 0x200000
#define CLONE_CHILD_SETTID 0x1000000
#define CLONE_UNSUPPORTED 0x7e02e000UL /* namespaces, ptrace, io, pidfd... */

static void setup_child_stack(struct task *t, struct regs *r) {
  *t->regs = *r;
  uint64_t *sp = (uint64_t *)t->regs;
  *--sp = (uint64_t)ret_from_fork;
  for (int i = 0; i < 6; i++)
    *--sp = 0;
  t->ksp = (uint64_t)sp;
}

long do_fork(uint64_t flags, uint64_t stack, uint64_t ptid, uint64_t ctid, uint64_t tls, struct regs *r) {
  struct proc *parent = cur_proc();
  if ((flags & CLONE_THREAD) && (!(flags & CLONE_SIGHAND) || !(flags & CLONE_VM)))
    return -EINVAL;
  if ((flags & CLONE_SIGHAND) && !(flags & CLONE_VM))
    return -EINVAL;
  if (flags & (CLONE_UNSUPPORTED & ~(uint64_t)CLONE_PIDFD))
    return -EINVAL;
  if (flags & CLONE_PIDFD)
    return -EINVAL;
  fxsave(current->fpu);
  struct task *t;
  struct proc *p;
  if (flags & CLONE_THREAD) {
    p = parent;
    t = task_alloc(p);
    if (!t)
      return -EAGAIN;
    list_add_tail(&p->threads, &t->thread_link);
    p->nthreads++;
  } else {
    int nproc = 0;
    list_for_each(q, &all_procs) nproc++;
    if (nproc >= 4096 && !capable())
      return -EAGAIN;
    p = proc_alloc();
    if (!p)
      return -ENOMEM;
    t = task_alloc(p);
    if (!t) {
      kfree(p);
      return -EAGAIN;
    }
    p->pid = t->tid;
    if (flags & CLONE_VM) {
      p->mm = parent->mm;
      p->mm->ref++;
    } else {
      p->mm = mm_dup(parent->mm);
      if (!p->mm) {
        list_del(&t->all_link);
        task_free(t);
        kfree(p);
        return -ENOMEM;
      }
    }
    if (fds_dup(p, parent)) {
      mm_put(p->mm);
      list_del(&t->all_link);
      task_free(t);
      kfree(p);
      return -EMFILE;
    }
    p->cwd = parent->cwd;
    path_get(&p->cwd);
    p->root = parent->root;
    path_get(&p->root);
    p->umask = parent->umask;
    memcpy(p->sigact, parent->sigact, sizeof(p->sigact));
    p->cred = parent->cred;
    if (p->cred.ngroups) {
      p->cred.groups = kmalloc(p->cred.ngroups * sizeof(gid_t));
      memcpy(p->cred.groups, parent->cred.groups, p->cred.ngroups * sizeof(gid_t));
    }
    memcpy(p->rlim, parent->rlim, sizeof(p->rlim));
    memcpy(p->comm, parent->comm, sizeof(p->comm));
    p->exe = parent->exe ? kstrdup(parent->exe) : NULL;
    p->pgid = parent->pgid;
    p->sid = parent->sid;
    p->ctty = parent->ctty;
    p->nice = parent->nice;
    p->exit_status = (int)(flags & 0xff); /* the exit signal, until exit */
    struct proc *pp = (flags & CLONE_PARENT) ? parent->parent : parent;
    p->parent = pp;
    list_add_tail(&pp->children, &p->sibling);
    list_add_tail(&p->threads, &t->thread_link);
    p->nthreads = 1;
    list_add_tail(&all_procs, &p->all_link);
  }
  t->blocked = current->blocked;
  t->policy = current->policy;
  t->rtprio = current->rtprio;
  ss_copy(t, current);
  memcpy(t->fpu, current->fpu, 512);
  t->fsbase = (flags & CLONE_SETTLS) ? tls : current->fsbase;
  if (!(flags & CLONE_VM)) /* threads do not inherit the alternate stack */
    t->altstack = current->altstack;
  setup_child_stack(t, r);
  t->regs->rax = 0;
  if (stack)
    t->regs->rsp = stack;
  if (flags & CLONE_CHILD_CLEARTID)
    t->clear_child_tid = ctid;
  int32_t tid = t->tid;
  if (flags & CLONE_PARENT_SETTID)
    copy_to_user((void *)ptid, &tid, 4);
  if (flags & CLONE_CHILD_SETTID) {
    if (flags & CLONE_VM)
      copy_to_user((void *)ctid, &tid, 4);
    else {
      /* write in the child's copy */
      mm_switch(p->mm);
      int e = handle_fault(p->mm, ctid, true, false);
      if (!e)
        copy_to_user((void *)ctid, &tid, 4);
      mm_switch(parent->mm);
    }
  }
  uint64_t f = irq_save();
  t->state = T_RUNNING;
  make_runnable(t);
  irq_restore(f);
  if (flags & CLONE_VFORK) {
    f = irq_save();
    while (!p->vfork_done)
      sleep_on(&p->vfork_wait, 0, false);
    irq_restore(f);
  }
  return tid;
}

long sys_clone(long flags, long stack, long ptid, long ctid, long tls) {
  return do_fork((uint64_t)flags, (uint64_t)stack, (uint64_t)ptid, (uint64_t)ctid, (uint64_t)tls, current->regs);
}

long sys_fork(void) { return do_fork(SIGCHLD, 0, 0, 0, 0, current->regs); }

long sys_vfork(void) { return do_fork(CLONE_VM | CLONE_VFORK | SIGCHLD, 0, 0, 0, 0, current->regs); }

struct clone_args {
  uint64_t flags, pidfd, child_tid, parent_tid, exit_signal, stack, stack_size, tls;
};

long sys_clone3(long uargs, long size) {
  struct clone_args a;
  memset(&a, 0, sizeof(a));
  if (size < 64 || (size_t)size > 4096)
    return -EINVAL;
  if (copy_from_user(&a, (void *)uargs, sizeof(a)))
    return -EFAULT;
  if (a.exit_signal & ~0xffUL)
    return -EINVAL;
  return do_fork(a.flags | a.exit_signal, a.stack ? a.stack + a.stack_size : 0, a.parent_tid,
                 a.child_tid, a.tls, current->regs);
}

/* ---- exit ---- */

static void vfork_release(struct proc *p) {
  if (!p->vfork_done) {
    p->vfork_done = true;
    wake_up(&p->vfork_wait);
  }
}

void proc_vfork_release(struct proc *p) { vfork_release(p); }

static void proc_free(struct proc *p) {
  list_del(&p->sibling);
  list_del(&p->all_link);
  pending_clear(&p->pending);
  kfree(p->cred.groups);
  kfree(p->exe);
  kfree(p);
}

/* the parent is told about a dead child: SIGCHLD and a wakeup */
static void notify_parent(struct proc *p) {
  struct proc *pp = p->parent;
  int sig = p->exit_status >> 16; /* kept there by proc_exit */
  int status = p->exit_status & 0xffff;
  p->exit_status = status;
  bool autoreap = pp->sigact[SIGCHLD - 1].handler == SIG_IGN ||
                  (pp->sigact[SIGCHLD - 1].flags & SA_NOCLDWAIT);
  if (sig) {
    struct siginfo si;
    memset(&si, 0, sizeof(si));
    si.signo = sig;
    if ((status & 0x7f) == 0) {
      si.code = CLD_EXITED;
      si.u.chld.status = (status >> 8) & 0xff;
    } else {
      si.code = (status & 0x80) ? CLD_DUMPED : CLD_KILLED;
      si.u.chld.status = status & 0x7f;
    }
    si.u.chld.pid = p->pid;
    si.u.chld.uid = p->cred.uid;
    si.u.chld.utime = (int64_t)(p->utime * 100 / HZ);
    si.u.chld.stime = (int64_t)(p->stime * 100 / HZ);
    send_signal_proc(pp, &si);
  }
  if (autoreap && pp != p) {
    p->state = P_DEAD;
    proc_free(p);
  }
  wake_up(&pp->child_wait);
}

/* the last thread of p has exited */
static void proc_exit(struct proc *p, int status) {
  int exitsig = p->exit_status & 0xff;
  if (p == init_proc)
    panic("init exited (status %#x)", status);
  timers_exit(p);
  fd_close_all(p);
  path_put(&p->cwd);
  path_put(&p->root);
  sem_exit(p);
  struct mm *mm = p->mm;
  p->mm = NULL;
  mm_switch(NULL);
  mm_put(mm);
  vfork_release(p);
  /* a session leader's terminal is hung up */
  if (p->sid == p->pid && p->ctty)
    tty_session_hangup(p->ctty, p->sid);
  orphan_check_exit(p);
  /* children go to init */
  list_for_each_safe(c, n, &p->children) {
    struct proc *ch = list_entry(c, struct proc, sibling);
    list_del(&ch->sibling);
    ch->parent = init_proc;
    list_add_tail(&init_proc->children, &ch->sibling);
    if (ch->state == P_ZOMBIE)
      wake_up(&init_proc->child_wait);
  }
  p->state = P_ZOMBIE;
  p->exit_status = status | exitsig << 16;
  notify_parent(p);
}

NORETURN void do_exit(int status) {
  struct task *t = current;
  struct proc *p = t->proc;
  t->exiting = true;
  if (t->clear_child_tid) {
    int32_t z = 0;
    if (copy_to_user((void *)t->clear_child_tid, &z, 4) == 0)
      futex_wake_addr(t->clear_child_tid, 1);
  }
  exit_robust_list(t);
  cli();
  list_del(&t->thread_link);
  p->nthreads--;
  /* process-directed signals that this thread still holds stay in p */
  if (p->nthreads == 0) {
    sti();
    proc_exit(p, p->group_exit ? p->group_status : status);
    cli();
  }
  t->state = T_ZOMBIE;
  list_del(&t->all_link);
  task_set_dead(t);
  schedule();
  panic("zombie task ran");
}

NORETURN void do_group_exit(int status) {
  struct proc *p = cur_proc();
  if (!p->group_exit) {
    p->group_exit = true;
    p->group_status = status;
    list_for_each(q, &p->threads) {
      struct task *o = list_entry(q, struct task, thread_link);
      if (o != current) {
        o->pending.set |= SIGBIT(SIGKILL);
        wake_task(o);
        if (o->state == T_STOPPED) {
          o->state = T_RUNNING;
          make_runnable(o);
        }
      }
    }
  }
  do_exit(status);
}

long sys_exit(long code) { do_exit(((int)code & 0xff) << 8); }

long sys_exit_group(long code) { do_group_exit(((int)code & 0xff) << 8); }

/* ---- wait ---- */

#define WNOHANG 1
#define WUNTRACED 2
#define WEXITED 4
#define WCONTINUED 8
#define WNOWAIT 0x01000000
#define WALL 0x40000000
#define WCLONE 0x80000000

#define P_ALL 0
#define P_PID 1
#define P_PGID 2
#define P_PIDFD 3

struct krusage {
  int64_t utime[2], stime[2];
  int64_t rest[14];
};

static void fill_rusage(struct krusage *ru, uint64_t ut, uint64_t st) {
  memset(ru, 0, sizeof(*ru));
  ru->utime[0] = (int64_t)(ut / HZ);
  ru->utime[1] = (int64_t)(ut % HZ * (1000000 / HZ));
  ru->stime[0] = (int64_t)(st / HZ);
  ru->stime[1] = (int64_t)(st % HZ * (1000000 / HZ));
}

/* which: P_ALL, P_PID, P_PGID.  Fills *status / info, returns the pid, 0
   (WNOHANG, nothing yet) or an error. */
static long do_wait(int which, pid_t id, int options, int *status, struct siginfo *info,
                    struct krusage *ru) {
  struct proc *self = cur_proc();
  uint64_t f = irq_save();
  bool intr = false;
  for (;;) {
    bool any = false;
    list_for_each_safe(c, n, &self->children) {
      struct proc *ch = list_entry(c, struct proc, sibling);
      if (which == P_PID && ch->pid != id)
        continue;
      if (which == P_PGID && ch->pgid != id)
        continue;
      if (ch->state == P_DEAD)
        continue;
      any = true;
      if (ch->state == P_ZOMBIE && (options & WEXITED)) {
        pid_t pid = ch->pid;
        int st = ch->exit_status & 0xffff;
        *status = st;
        if (info) {
          info->signo = SIGCHLD;
          info->u.chld.pid = pid;
          info->u.chld.uid = ch->cred.uid;
          if ((st & 0x7f) == 0) {
            info->code = CLD_EXITED;
            info->u.chld.status = (st >> 8) & 0xff;
          } else {
            info->code = (st & 0x80) ? CLD_DUMPED : CLD_KILLED;
            info->u.chld.status = st & 0x7f;
          }
        }
        if (ru)
          fill_rusage(ru, ch->utime + ch->cutime, ch->stime + ch->cstime);
        if (!(options & WNOWAIT)) {
          self->cutime += ch->utime + ch->cutime;
          self->cstime += ch->stime + ch->cstime;
          ch->state = P_DEAD;
          proc_free(ch);
        }
        irq_restore(f);
        return pid;
      }
      if (ch->report == CLD_STOPPED && (options & WUNTRACED)) {
        *status = (ch->stop_sig << 8) | 0x7f;
        if (info) {
          info->signo = SIGCHLD;
          info->code = CLD_STOPPED;
          info->u.chld.pid = ch->pid;
          info->u.chld.uid = ch->cred.uid;
          info->u.chld.status = ch->stop_sig;
        }
        if (ru)
          fill_rusage(ru, ch->utime, ch->stime);
        if (!(options & WNOWAIT))
          ch->report = 0;
        irq_restore(f);
        return ch->pid;
      }
      if (ch->report == CLD_CONTINUED && (options & WCONTINUED)) {
        *status = 0xffff;
        if (info) {
          info->signo = SIGCHLD;
          info->code = CLD_CONTINUED;
          info->u.chld.pid = ch->pid;
          info->u.chld.uid = ch->cred.uid;
          info->u.chld.status = SIGCONT;
        }
        if (ru)
          fill_rusage(ru, ch->utime, ch->stime);
        if (!(options & WNOWAIT))
          ch->report = 0;
        irq_restore(f);
        return ch->pid;
      }
    }
    if (!any) {
      irq_restore(f);
      return -ECHILD;
    }
    if (options & WNOHANG) {
      irq_restore(f);
      return 0;
    }
    if (intr) {
      irq_restore(f);
      return -ERESTARTSYS;
    }
    /* interrupted: the children are looked at once more first (a child
       reaped automatically, SA_NOCLDWAIT, may have been the last: ECHILD) */
    if (sleep_on(&self->child_wait, 0, true) == -EINTR)
      intr = true;
  }
}

long sys_wait4(long pid, long ustatus, long options, long uru) {
  if (options & ~(WNOHANG | WUNTRACED | WCONTINUED | WALL | WCLONE))
    return -EINVAL;
  int which = P_PID;
  pid_t id = (pid_t)pid;
  if (pid == -1)
    which = P_ALL;
  else if (pid == 0) {
    which = P_PGID;
    id = cur_proc()->pgid;
  } else if (pid < -1) {
    which = P_PGID;
    id = (pid_t)-pid;
  }
  int status = 0;
  struct krusage ru;
  long r = do_wait(which, id, (int)options | WEXITED, &status, NULL, uru ? &ru : NULL);
  if (r > 0) {
    if (ustatus && copy_to_user((void *)ustatus, &status, 4))
      return -EFAULT;
    if (uru && copy_to_user((void *)uru, &ru, sizeof(ru)))
      return -EFAULT;
  }
  return r;
}

long sys_waitid(long which, long id, long uinfo, long options, long uru) {
  if (options & ~(WNOHANG | WUNTRACED | WEXITED | WCONTINUED | WNOWAIT | WALL | WCLONE))
    return -EINVAL;
  if (!(options & (WEXITED | WUNTRACED | WCONTINUED)))
    return -EINVAL;
  if (which == P_PGID && id == 0)
    id = cur_proc()->pgid;
  if (which != P_ALL && which != P_PID && which != P_PGID)
    return -EINVAL;
  struct siginfo si;
  memset(&si, 0, sizeof(si));
  int status;
  struct krusage ru;
  long r = do_wait((int)which, (pid_t)id, (int)options, &status, &si, uru ? &ru : NULL);
  if (r < 0)
    return r;
  if (uinfo && copy_to_user((void *)uinfo, &si, sizeof(si)))
    return -EFAULT;
  if (uru && copy_to_user((void *)uru, &ru, sizeof(ru)))
    return -EFAULT;
  return 0;
}

/* ---- ids, groups, sessions ---- */

long sys_getpid(void) { return cur_proc()->pid; }
long sys_gettid(void) { return current->tid; }
long sys_getppid(void) {
  struct proc *p = cur_proc();
  return p == init_proc ? 0 : p->parent->pid;
}
long sys_getpgrp(void) { return cur_proc()->pgid; }

long sys_getpgid(long pid) {
  struct proc *p = pid ? find_proc((pid_t)pid) : cur_proc();
  if (!p)
    return -ESRCH;
  return p->pgid;
}

long sys_getsid(long pid) {
  struct proc *p = pid ? find_proc((pid_t)pid) : cur_proc();
  if (!p)
    return -ESRCH;
  return p->sid;
}

long sys_setsid(void) {
  struct proc *p = cur_proc();
  list_for_each(q, &all_procs) {
    struct proc *o = list_entry(q, struct proc, all_link);
    if (o->state != P_DEAD && o->pgid == p->pid)
      return -EPERM;
  }
  p->sid = p->pgid = p->pid;
  p->ctty = NULL;
  return p->sid;
}

long sys_setpgid(long pid, long pgid) {
  struct proc *self = cur_proc();
  struct proc *p = pid ? find_proc((pid_t)pid) : self;
  if (pgid < 0)
    return -EINVAL;
  if (!p)
    return -ESRCH;
  if (p != self) {
    if (p->parent != self)
      return -ESRCH;
    if (p->sid != self->sid)
      return -EPERM;
    if (p->did_exec)
      return -EACCES;
  }
  if (p->sid == p->pid)
    return -EPERM;
  if (!pgid)
    pgid = p->pid;
  if (pgid != p->pid) {
    bool ok = false;
    list_for_each(q, &all_procs) {
      struct proc *o = list_entry(q, struct proc, all_link);
      if (o->state == P_ALIVE && o->pgid == pgid && o->sid == p->sid)
        ok = true;
    }
    if (!ok)
      return -EPERM;
  }
  p->pgid = (pid_t)pgid;
  return 0;
}

long sys_getuid(void) { return cur_proc()->cred.uid; }
long sys_geteuid(void) { return cur_proc()->cred.euid; }
long sys_getgid(void) { return cur_proc()->cred.gid; }
long sys_getegid(void) { return cur_proc()->cred.egid; }

long sys_getresuid(long r, long e, long s) {
  struct cred *c = &cur_proc()->cred;
  if (copy_to_user((void *)r, &c->uid, 4) || copy_to_user((void *)e, &c->euid, 4) ||
      copy_to_user((void *)s, &c->suid, 4))
    return -EFAULT;
  return 0;
}

long sys_getresgid(long r, long e, long s) {
  struct cred *c = &cur_proc()->cred;
  if (copy_to_user((void *)r, &c->gid, 4) || copy_to_user((void *)e, &c->egid, 4) ||
      copy_to_user((void *)s, &c->sgid, 4))
    return -EFAULT;
  return 0;
}

#define KEEP ((uint32_t)-1)

static long set_res(uint32_t *real, uint32_t *eff, uint32_t *saved, uint32_t *fs, long r, long e,
                    long s, bool priv) {
  uint32_t nr = (uint32_t)r, ne = (uint32_t)e, ns = (uint32_t)s;
  if (!priv) {
    if (nr != KEEP && nr != *real && nr != *eff && nr != *saved)
      return -EPERM;
    if (ne != KEEP && ne != *real && ne != *eff && ne != *saved)
      return -EPERM;
    if (ns != KEEP && ns != *real && ns != *eff && ns != *saved)
      return -EPERM;
  }
  if (nr != KEEP)
    *real = nr;
  if (ne != KEEP)
    *eff = ne;
  if (ns != KEEP)
    *saved = ns;
  *fs = *eff;
  return 0;
}

long sys_setresuid(long r, long e, long s) {
  struct cred *c = &cur_proc()->cred;
  return set_res(&c->uid, &c->euid, &c->suid, &c->fsuid, r, e, s, c->euid == 0);
}

long sys_setresgid(long r, long e, long s) {
  struct cred *c = &cur_proc()->cred;
  return set_res(&c->gid, &c->egid, &c->sgid, &c->fsgid, r, e, s, c->euid == 0);
}

long sys_setuid(long uid) {
  struct cred *c = &cur_proc()->cred;
  if (c->euid == 0)
    return set_res(&c->uid, &c->euid, &c->suid, &c->fsuid, uid, uid, uid, true);
  if ((uint32_t)uid != c->uid && (uint32_t)uid != c->suid)
    return -EPERM;
  c->euid = c->fsuid = (uint32_t)uid;
  return 0;
}

long sys_setgid(long gid) {
  struct cred *c = &cur_proc()->cred;
  if (c->euid == 0)
    return set_res(&c->gid, &c->egid, &c->sgid, &c->fsgid, gid, gid, gid, true);
  if ((uint32_t)gid != c->gid && (uint32_t)gid != c->sgid)
    return -EPERM;
  c->egid = c->fsgid = (uint32_t)gid;
  return 0;
}

long sys_setreuid(long r, long e) {
  struct cred *c = &cur_proc()->cred;
  uint32_t nr = (uint32_t)r, ne = (uint32_t)e;
  if (c->euid != 0) {
    if (nr != KEEP && nr != c->uid && nr != c->euid)
      return -EPERM;
    if (ne != KEEP && ne != c->uid && ne != c->euid && ne != c->suid)
      return -EPERM;
  }
  uint32_t oldr = c->uid;
  if (nr != KEEP)
    c->uid = nr;
  if (ne != KEEP)
    c->euid = ne;
  if (nr != KEEP || (ne != KEEP && ne != oldr))
    c->suid = c->euid;
  c->fsuid = c->euid;
  return 0;
}

long sys_setregid(long r, long e) {
  struct cred *c = &cur_proc()->cred;
  uint32_t nr = (uint32_t)r, ne = (uint32_t)e;
  if (c->euid != 0) {
    if (nr != KEEP && nr != c->gid && nr != c->egid)
      return -EPERM;
    if (ne != KEEP && ne != c->gid && ne != c->egid && ne != c->sgid)
      return -EPERM;
  }
  uint32_t oldr = c->gid;
  if (nr != KEEP)
    c->gid = nr;
  if (ne != KEEP)
    c->egid = ne;
  if (nr != KEEP || (ne != KEEP && ne != oldr))
    c->sgid = c->egid;
  c->fsgid = c->egid;
  return 0;
}

long sys_setfsuid(long uid) {
  struct cred *c = &cur_proc()->cred;
  uint32_t old = c->fsuid;
  if (c->euid == 0 || (uint32_t)uid == c->uid || (uint32_t)uid == c->euid || (uint32_t)uid == c->suid)
    c->fsuid = (uint32_t)uid;
  return old;
}

long sys_setfsgid(long gid) {
  struct cred *c = &cur_proc()->cred;
  uint32_t old = c->fsgid;
  if (c->euid == 0 || (uint32_t)gid == c->gid || (uint32_t)gid == c->egid || (uint32_t)gid == c->sgid)
    c->fsgid = (uint32_t)gid;
  return old;
}

long sys_getgroups(long n, long ulist) {
  struct cred *c = &cur_proc()->cred;
  if (n < 0)
    return -EINVAL;
  if (n == 0)
    return c->ngroups;
  if (n < c->ngroups)
    return -EINVAL;
  if (copy_to_user((void *)ulist, c->groups, c->ngroups * sizeof(gid_t)))
    return -EFAULT;
  return c->ngroups;
}

long sys_setgroups(long n, long ulist) {
  struct cred *c = &cur_proc()->cred;
  if (!capable())
    return -EPERM;
  if (n < 0 || n > NGROUPS_MAX)
    return -EINVAL;
  gid_t *g = n ? kmalloc(n * sizeof(gid_t)) : NULL;
  if (n && !g)
    return -ENOMEM;
  if (n && copy_from_user(g, (void *)ulist, n * sizeof(gid_t))) {
    kfree(g);
    return -EFAULT;
  }
  kfree(c->groups);
  c->groups = g;
  c->ngroups = (int)n;
  return 0;
}

bool in_group(gid_t g) {
  struct cred *c = &cur_proc()->cred;
  if (g == c->fsgid)
    return true;
  for (int i = 0; i < c->ngroups; i++)
    if (c->groups[i] == g)
      return true;
  return false;
}

/* ---- limits, usage, misc ---- */

long sys_umask(long m) {
  struct proc *p = cur_proc();
  long old = p->umask;
  p->umask = (mode_t)m & 0777;
  return old;
}

long sys_prlimit64(long pid, long res, long unew, long uold) {
  struct proc *p = pid ? find_proc((pid_t)pid) : cur_proc();
  if (!p)
    return -ESRCH;
  if (res < 0 || res >= RLIM_NLIMITS)
    return -EINVAL;
  struct rlimit64 n, o = p->rlim[res];
  if (unew) {
    if (copy_from_user(&n, (void *)unew, sizeof(n)))
      return -EFAULT;
    if (n.cur > n.max)
      return -EINVAL;
    if (n.max > p->rlim[res].max && !capable())
      return -EPERM;
    if (res == RLIMIT_NOFILE && n.max > 1024 * 1024)
      return -EPERM;
  }
  if (uold && copy_to_user((void *)uold, &o, sizeof(o)))
    return -EFAULT;
  if (unew)
    p->rlim[res] = n;
  return 0;
}

long sys_getrlimit(long res, long u) { return sys_prlimit64(0, res, 0, u); }
long sys_setrlimit(long res, long u) { return sys_prlimit64(0, res, u, 0); }

long sys_getrusage(long who, long uru) {
  struct proc *p = cur_proc();
  struct krusage ru;
  if (who == 0)
    fill_rusage(&ru, p->utime, p->stime);
  else if (who == -1)
    fill_rusage(&ru, p->cutime, p->cstime);
  else if (who == 1)
    fill_rusage(&ru, current->utime, current->stime);
  else
    return -EINVAL;
  return copy_to_user((void *)uru, &ru, sizeof(ru)) ? -EFAULT : 0;
}

/* priority: process-wide nice (POSIX); Linux returns 20 - nice */
#define PRIO_PROCESS 0
#define PRIO_PGRP 1
#define PRIO_USER 2

long sys_getpriority(long which, long who) {
  int best = 100;
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state != P_ALIVE)
      continue;
    bool m = which == PRIO_PROCESS ? p->pid == (who ? who : cur_proc()->pid)
           : which == PRIO_PGRP    ? p->pgid == (who ? who : cur_proc()->pgid)
           : which == PRIO_USER    ? p->cred.uid == (uint32_t)(who ? who : cur_proc()->cred.uid)
                                   : false;
    if (m && 20 - p->nice < 100 && (best == 100 || 20 - p->nice > best))
      best = 20 - p->nice;
  }
  if (which < 0 || which > 2)
    return -EINVAL;
  return best == 100 ? -ESRCH : best;
}

long sys_setpriority(long which, long who, long nice) {
  if (which < 0 || which > 2)
    return -EINVAL;
  if (nice < -20)
    nice = -20;
  if (nice > 19)
    nice = 19;
  bool found = false;
  struct cred *me = &cur_proc()->cred;
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state != P_ALIVE)
      continue;
    bool m = which == PRIO_PROCESS ? p->pid == (who ? who : cur_proc()->pid)
           : which == PRIO_PGRP    ? p->pgid == (who ? who : cur_proc()->pgid)
                                   : p->cred.uid == (uint32_t)(who ? who : me->uid);
    if (!m)
      continue;
    found = true;
    if (me->euid != 0 && me->euid != p->cred.uid && me->euid != p->cred.euid)
      return -EPERM;
    if (nice < p->nice && me->euid != 0)
      return -EACCES;
    p->nice = (int)nice;
  }
  return found ? 0 : -ESRCH;
}

long sys_set_tid_address(long a) {
  current->clear_child_tid = (uint64_t)a;
  return current->tid;
}

long sys_set_robust_list(long head, long len) {
  if (len != 24)
    return -EINVAL;
  current->robust_list = (uint64_t)head;
  return 0;
}

long sys_get_robust_list(long pid, long uhead, long ulen) {
  struct task *t = pid ? find_task((pid_t)pid) : current;
  if (!t)
    return -ESRCH;
  uint64_t len = 24;
  if (copy_to_user((void *)uhead, &t->robust_list, 8) || copy_to_user((void *)ulen, &len, 8))
    return -EFAULT;
  return 0;
}

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

long sys_arch_prctl(long code, long addr) {
  switch (code) {
  case ARCH_SET_FS:
    if ((uint64_t)addr >= USER_TOP)
      return -EPERM;
    current->fsbase = (uint64_t)addr;
    wrmsr(MSR_FS_BASE, (uint64_t)addr);
    return 0;
  case ARCH_GET_FS:
    return copy_to_user((void *)addr, &current->fsbase, 8) ? -EFAULT : 0;
  case ARCH_SET_GS:
    return 0;
  case ARCH_GET_GS: {
    uint64_t z = 0;
    return copy_to_user((void *)addr, &z, 8) ? -EFAULT : 0;
  }
  }
  return -EINVAL;
}

#define PR_SET_PDEATHSIG 1
#define PR_GET_PDEATHSIG 2
#define PR_GET_DUMPABLE 3
#define PR_SET_DUMPABLE 4
#define PR_SET_NAME 15
#define PR_GET_NAME 16

long sys_prctl(long op, long a2) {
  struct proc *p = cur_proc();
  switch (op) {
  case PR_SET_NAME: {
    char n[16];
    long r = strncpy_from_user(n, (const char *)a2, 16);
    if (r == -ENAMETOOLONG) {
      if (copy_from_user(n, (void *)a2, 15))
        return -EFAULT;
      n[15] = 0;
    } else if (r < 0)
      return r;
    memcpy(p->comm, n, 16);
    return 0;
  }
  case PR_GET_NAME:
    return copy_to_user((void *)a2, p->comm, 16) ? -EFAULT : 0;
  case PR_SET_PDEATHSIG:
  case PR_SET_DUMPABLE:
    return 0;
  case PR_GET_DUMPABLE:
    return 1;
  case PR_GET_PDEATHSIG: {
    int z = 0;
    return copy_to_user((void *)a2, &z, 4) ? -EFAULT : 0;
  }
  }
  return -EINVAL;
}

long sys_personality(long v) {
  (void)v;
  return 0;
}

/* ---- scheduling parameters (one processor, round robin for all) ---- */

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_IDLE 5

static struct task *sched_target(long pid) {
  if (pid < 0)
    return NULL;
  return pid ? find_task((pid_t)pid) : current;
}

long sys_sched_getscheduler(long pid) {
  if (pid < 0)
    return -EINVAL;
  struct task *t = sched_target(pid);
  return t ? t->policy : -ESRCH;
}

long sys_sched_getparam(long pid, long up) {
  if (pid < 0 || !up)
    return -EINVAL;
  struct task *t = sched_target(pid);
  if (!t)
    return -ESRCH;
  if (t->policy == SCHED_SPORADIC) { /* the long struct sched_param */
    struct ss_param sp;
    ss_get(t, &sp);
    return copy_to_user((void *)up, &sp, sizeof(sp)) ? -EFAULT : 0;
  }
  int32_t prio = t->rtprio;
  return copy_to_user((void *)up, &prio, 4) ? -EFAULT : 0;
}

static long set_sched(long pid, int policy, long up) {
  if (pid < 0 || !up)
    return -EINVAL;
  struct task *t = sched_target(pid);
  if (!t)
    return -ESRCH;
  int32_t prio;
  if (copy_from_user(&prio, (void *)up, 4))
    return -EFAULT;
  if (policy < 0)
    policy = t->policy;
  policy &= ~0x40000000; /* SCHED_RESET_ON_FORK */
  if (policy == SCHED_SPORADIC) {
    struct ss_param sp;
    if (copy_from_user(&sp, (void *)up, sizeof(sp)))
      return -EFAULT;
    if (!capable())
      return -EPERM;
    struct cred *me = &cur_proc()->cred;
    if (me->euid != 0 && me->euid != t->proc->cred.uid && me->euid != t->proc->cred.euid)
      return -EPERM;
    return ss_set(t, &sp);
  }
  if (policy == SCHED_FIFO || policy == SCHED_RR) {
    if (prio < 1 || prio > 99)
      return -EINVAL;
    if (!capable())
      return -EPERM;
  } else if (policy == SCHED_OTHER || policy == SCHED_BATCH || policy == SCHED_IDLE) {
    if (prio != 0)
      return -EINVAL;
  } else
    return -EINVAL;
  struct cred *me = &cur_proc()->cred;
  if (me->euid != 0 && me->euid != t->proc->cred.uid && me->euid != t->proc->cred.euid)
    return -EPERM;
  ss_free(t);
  t->policy = policy;
  t->rtprio = prio;
  task_prio_changed(t);
  return 0;
}

long sys_sched_setscheduler(long pid, long policy, long up) {
  if (policy < 0)
    return -EINVAL;
  return set_sched(pid, (int)policy, up);
}
long sys_sched_setparam(long pid, long up) { return set_sched(pid, -1, up); }

long sys_sched_get_priority_max(long policy) {
  return policy == SCHED_FIFO || policy == SCHED_RR || policy == SCHED_SPORADIC ? 99 : policy == SCHED_OTHER || policy == SCHED_BATCH || policy == SCHED_IDLE ? 0 : -EINVAL;
}
long sys_sched_get_priority_min(long policy) {
  return policy == SCHED_FIFO || policy == SCHED_RR || policy == SCHED_SPORADIC ? 1 : policy == SCHED_OTHER || policy == SCHED_BATCH || policy == SCHED_IDLE ? 0 : -EINVAL;
}

long sys_sched_rr_get_interval(long pid, long uts) {
  if (pid < 0)
    return -EINVAL;
  if (!sched_target(pid))
    return -ESRCH;
  struct timespec64 ts = {0, 10000000};
  return copy_to_user((void *)uts, &ts, sizeof(ts)) ? -EFAULT : 0;
}

long sys_sched_yield(void) {
  yield();
  return 0;
}

long sys_sched_getaffinity(long pid, long len, long umask) {
  if (!sched_target(pid))
    return -ESRCH;
  if (len < 8 || (len & 7))
    return -EINVAL;
  uint64_t m = 1;
  if (clear_user((void *)umask, (size_t)len) || copy_to_user((void *)umask, &m, 8))
    return -EFAULT;
  return 8;
}

long sys_sched_setaffinity(long pid, long len, long umask) {
  (void)len;
  (void)umask;
  return sched_target(pid) ? 0 : -ESRCH;
}

long sys_getcpu(long ucpu, long unode) {
  uint32_t z = 0;
  if (ucpu && copy_to_user((void *)ucpu, &z, 4))
    return -EFAULT;
  if (unode && copy_to_user((void *)unode, &z, 4))
    return -EFAULT;
  return 0;
}

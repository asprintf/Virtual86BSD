/* proc.h - tasks (threads), processes (thread groups), scheduling. */
#ifndef _SYS_PROC_H_
#define _SYS_PROC_H_

#include <sys/systm.h>
#include <sys/signalvar.h>

struct regs;
struct mm;
struct file;
struct inode;
struct mount;
struct tty;

struct waitq {
  struct list head;
};
#define WAITQ_INIT(q) { LIST_INIT((q).head) }
static inline void waitq_init(struct waitq *q) { list_init(&q->head); }

struct sigpending {
  sigset_t set;
  struct list queue;
};

enum { T_RUNNING, T_SLEEPING, T_UNINTERRUPTIBLE, T_STOPPED, T_ZOMBIE };

struct task {
  pid_t tid;
  struct proc *proc;
  int state;
  struct list thread_link; /* proc->threads */
  struct list run_link;    /* run queue */
  struct list all_link;    /* all tasks */
  void *kstack;
  uint64_t ksp;
  struct regs *regs;       /* user frame at the top of kstack */
  uint64_t fsbase;
  void *fpu;               /* fxsave area */
  bool is_kthread;

  sigset_t blocked, saved_mask;
  bool restore_mask;
  /* a relative nanosleep stopped by a signal without a handler (job
     control) goes on to its first deadline when restarted */
  bool sleep_restart;
  int sleep_restart_clk;
  long sleep_restart_req;
  uint64_t sleep_restart_abs;
  struct sigpending pending;
  struct sigaltstack altstack;

  uint64_t clear_child_tid, robust_list;

  /* sleeping */
  struct waitq *waitq;
  struct list wait_link;
  uint64_t wake_at;        /* monotonic ns, 0 = no timeout */
  struct list timer_link;
  bool timed_out;

  /* futex */
  uint64_t fkey1, fkey2;
  uint32_t fbitset;
  bool fwoken;
  struct list futex_link;
  int fprio; /* task_prio when it queued on a futex */

  int slice;
  uint64_t utime, stime;   /* ticks */
  int policy, rtprio;
  int boost;               /* priority inherited through PI futexes */
  struct ss *ss;           /* SCHED_SPORADIC (kern/sched.c) */
  bool exiting;
};

struct rlimit64 {
  uint64_t cur, max;
};
#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIMIT_LOCKS 10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE 13
#define RLIMIT_RTPRIO 14
#define RLIMIT_RTTIME 15
#define RLIM_NLIMITS 16
#define RLIM_INFINITY (~0UL)

#define NGROUPS_MAX 65536

/* a position in the file tree: a mount and an inode in it */
struct path {
  struct mount *mnt;
  struct inode *ino;
};

struct cred {
  uid_t uid, euid, suid, fsuid;
  gid_t gid, egid, sgid, fsgid;
  int ngroups;
  gid_t *groups;
};

enum { P_ALIVE, P_ZOMBIE, P_DEAD };

struct itimer {
  uint64_t expires;  /* monotonic ns, 0 = off */
  uint64_t interval;
};

/* A system call table: the calls a process of one ABI can make (the native
   one, kern/init_sysent.c; compat/linux's).  Each call takes the six
   argument registers and returns the result or -errno. */
typedef long (*sy_call_t)(long, long, long, long, long, long);
struct sysent {
  sy_call_t sy_call;
};

/* An emulation: how a process sees the kernel (struct proc's p_emul),
   chosen by exec from the program's brand. */
struct emul {
  const char *e_name;
  const struct sysent *e_sysent;
  int e_nsysent;
  int e_sigreturn;           /* the number of sigreturn: the restored rax stands */
  const char *e_ostype;      /* uname -s */
  const char *e_osrelease;   /* uname -r */
};
extern const struct emul emul_native;
#ifdef COMPAT_LINUX
extern const struct emul emul_linux;
#endif

struct proc {
  pid_t pid;
  const struct emul *p_emul;
  int state;
  struct proc *parent;
  struct list children, sibling;
  struct list threads;
  int nthreads;
  struct list all_link;
  pid_t pgid, sid;
  struct tty *ctty;
  struct mm *mm;

  struct file **fd;
  uint8_t *fdflags;  /* FD_CLOEXEC */
  int nfd;
  struct path cwd, root;
  mode_t umask;

  struct k_sigaction sigact[NSIG];
  struct sigpending pending;  /* process-directed */
  struct cred cred;
  struct rlimit64 rlim[RLIM_NLIMITS];
  char comm[16];
  char *exe;                  /* absolute path of the program */

  int exit_status;            /* wait status */
  bool group_exit;
  int group_status;
  bool did_exec;              /* for setpgid's EACCES */
  int stop_sig;               /* group stop in progress/done: the signal */
  bool stopped;
  int report;                 /* to the parent: 0, CLD_STOPPED, CLD_CONTINUED */
  struct waitq child_wait;    /* parent's wait4 sleeps here */
  struct waitq vfork_wait;
  bool vfork_done;

  uint64_t utime, stime, cutime, cstime; /* ticks */
  uint64_t start_time;        /* monotonic ns */
  struct itimer itimer[3];
  struct list timers;         /* POSIX timers */
  int next_timer_id;
  int nice;
};

extern struct task *current;
extern struct proc *init_proc;
extern struct list all_tasks, all_procs;
extern bool need_resched;

/* SCHED_SPORADIC: POSIX's sporadic server (kern/sched.c); the policy
   number is one Linux does not use, and struct sched_param the long form */
#define SCHED_SPORADIC 4
#define SS_REPL_MAX 32
struct ss_param {
  int32_t prio, low;
  int64_t period_s, period_ns, budget_s, budget_ns;
  int32_t max_repl, pad;
};
int ss_set(struct task *t, const struct ss_param *p); /* checked; the policy is set */
void ss_get(struct task *t, struct ss_param *p);
void ss_copy(struct task *dst, struct task *src);
void ss_free(struct task *t);

/* kern/sched.c */
void sched_init(void);
void schedule(void);  /* interrupts must be off */
void yield(void);
void wake_task(struct task *t);
void wake_up(struct waitq *q);
void wake_up_one(struct waitq *q);
int sleep_on(struct waitq *q, uint64_t deadline_ns, bool interruptible);
int sleep_ns(uint64_t ns);
void timer_tick(void);
struct task *kthread_create(const char *name, long (*fn)(void *), void *arg);
void make_runnable(struct task *t);
int task_prio(struct task *t); /* 0 normal, 1..99 real-time */
void task_prio_changed(struct task *t);
void kthread_exit(long code);
void schedule_tail(void);
void return_to_user(struct regs *r);

/* kern/proc.c */
struct task *task_alloc(struct proc *p);
void task_free(struct task *t);
struct proc *proc_alloc(void);
pid_t alloc_pid(void);
struct task *find_task(pid_t tid);
struct proc *find_proc(pid_t pid);
NORETURN void do_exit(int status);         /* this thread */
NORETURN void do_group_exit(int status);   /* the whole process */
void proc_vfork_release(struct proc *p);
bool in_group(gid_t g);
void orphan_check_exit(struct proc *p);
void sem_exit(struct proc *p);
long do_fork(uint64_t flags, uint64_t stack, uint64_t ptid, uint64_t ctid, uint64_t tls, struct regs *r);
void proc_init_first(void);
static inline struct proc *cur_proc(void) { return current->proc; }
bool capable(void);  /* effective uid 0 */

/* kern/exec.c */
int do_execve(const char *path, char *const *argv, char *const *envp, int dirfd, int flags);
int kernel_execve(const char *path, const char *const *argv, const char *const *envp);

/* kern/futex.c */
void futex_wake_addr(uint64_t uaddr, int n);
void exit_robust_list(struct task *t);

/* kern/time.c */
struct timespec64 {
  int64_t sec, nsec;
};
void itimer_tick(void);
void timers_exit(struct proc *p);
void timers_exit_posix(struct proc *p);

#endif

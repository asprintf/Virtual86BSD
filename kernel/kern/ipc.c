/* ipc.c - System V IPC: message queues, semaphore sets (with SEM_UNDO),
   shared memory segments.  Linux's 64-bit structures and numbering
   (identifier = sequence * 32768 + slot). */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "syscall.h"

#define IPC_PRIVATE 0
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2
#define IPC_INFO 3
#define MSG_STAT 11
#define MSG_INFO 12
#define MSG_STAT_ANY 13
#define MSG_NOERROR 010000
#define MSG_EXCEPT 020000
#define MSG_COPY 040000
#define GETPID 11
#define GETVAL 12
#define GETALL 13
#define GETNCNT 14
#define GETZCNT 15
#define SETVAL 16
#define SETALL 17
#define SEM_STAT 18
#define SEM_INFO 19
#define SEM_STAT_ANY 20
#define SEM_UNDO 0x1000
#define SHM_LOCK 11
#define SHM_UNLOCK 12
#define SHM_STAT 13
#define SHM_INFO 14
#define SHM_STAT_ANY 15
#define SHM_RDONLY 010000
#define SHM_RND 020000
#define SHM_REMAP 040000
#define SHM_EXEC 0100000
#define SHM_DEST 01000

#define NSLOT 256
#define MSGMAX 8192
#define MSGMNB 16384
#define SEMMSL 32000
#define SEMOPM 500
#define SEMVMX 32767
#define SHMMIN 1
#define SHMMAX (1UL << 40)

/* struct ipc64_perm */
struct kperm {
  int32_t key;
  uint32_t uid, gid, cuid, cgid, mode;
  uint16_t seq, pad2;
  uint32_t pad3;
  uint64_t unused1, unused2;
};

struct ipcobj {
  struct kperm perm;
  int id;
  bool used;
};

struct msg {
  struct list link;
  int64_t type;
  size_t len;
  char text[];
};

struct msq {
  struct ipcobj o;
  struct list msgs;
  uint64_t cbytes, qnum, qbytes;
  int32_t lspid, lrpid;
  int64_t stime, rtime, ctime;
  struct waitq wq;
};

struct semset {
  struct ipcobj o;
  int nsems;
  struct sem {
    int val;
    int32_t pid;
  } *sem;
  int64_t otime, ctime;
  struct waitq wq;
};

struct shmseg {
  struct ipcobj o;
  struct shmobj *obj;
  uint64_t size;
  int64_t atime, dtime, ctime;
  int32_t cpid, lpid;
  uint64_t nattch;
  bool dest;
};

static struct msq *msqs[NSLOT];
static struct semset *sems[NSLOT];
static struct shmseg *shms[NSLOT];
static uint16_t seqs[3][NSLOT];

static int64_t now_sec(void) { return (int64_t)(realtime_ns() / 1000000000UL); }

/* ---- common ---- */

static int ipc_perm_ok(struct kperm *p, int mask) {
  struct cred *c = &cur_proc()->cred;
  if (c->euid == 0)
    return 0;
  unsigned m;
  if (c->euid == p->uid || c->euid == p->cuid)
    m = (p->mode >> 6) & 7;
  else if (in_group(p->gid) || in_group(p->cgid))
    m = (p->mode >> 3) & 7;
  else
    m = p->mode & 7;
  return (m & (unsigned)mask) == (unsigned)mask ? 0 : -EACCES;
}

static bool is_owner(struct kperm *p) {
  struct cred *c = &cur_proc()->cred;
  return c->euid == 0 || c->euid == p->uid || c->euid == p->cuid;
}

static void perm_init(struct kperm *p, int32_t key, int flg, int kind, int slot) {
  struct cred *c = &cur_proc()->cred;
  memset(p, 0, sizeof(*p));
  p->key = key;
  p->uid = p->cuid = c->euid;
  p->gid = p->cgid = c->egid;
  p->mode = (uint32_t)flg & 0777;
  p->seq = seqs[kind][slot];
}

/* find by key in a table of ipcobj pointers (generic over the 3 tables) */
typedef struct ipcobj *(*getter)(int slot);
static struct ipcobj *g_msg(int i) { return msqs[i] ? &msqs[i]->o : NULL; }
static struct ipcobj *g_sem(int i) { return sems[i] ? &sems[i]->o : NULL; }
static struct ipcobj *g_shm(int i) { return shms[i] ? &shms[i]->o : NULL; }

static int find_key(getter g, int32_t key) {
  if (key == IPC_PRIVATE)
    return -1;
  for (int i = 0; i < NSLOT; i++) {
    struct ipcobj *o = g(i);
    if (o && o->perm.key == key && !(o->perm.mode & SHM_DEST))
      return i;
  }
  return -1;
}

static int free_slot(getter g) {
  for (int i = 0; i < NSLOT; i++)
    if (!g(i))
      return i;
  return -1;
}

static int make_id(int kind, int slot) { return seqs[kind][slot] * 32768 + slot; }

static int slot_of(getter g, int id) {
  if (id < 0)
    return -EINVAL;
  int slot = id % 32768;
  if (slot >= NSLOT)
    return -EINVAL;
  struct ipcobj *o = g(slot);
  if (!o || o->id != id)
    return -EINVAL;
  return slot;
}


static int perm_set(void *u, struct kperm *p) {
  struct kperm n;
  if (copy_from_user(&n, u, sizeof(n)))
    return -EFAULT;
  if (!is_owner(p))
    return -EPERM;
  p->uid = n.uid;
  p->gid = n.gid;
  p->mode = (p->mode & ~0777U) | (n.mode & 0777);
  return 0;
}

/* ---- message queues ---- */

long sys_msgget(long key, long flg) {
  int slot = find_key(g_msg, (int32_t)key);
  if (slot >= 0) {
    if ((flg & IPC_CREAT) && (flg & IPC_EXCL))
      return -EEXIST;
    struct msq *q = msqs[slot];
    int e = ipc_perm_ok(&q->o.perm, ((int)flg >> 6) & 7 & ~1);
    return e ? e : q->o.id;
  }
  if (!(flg & IPC_CREAT) && key != IPC_PRIVATE)
    return -ENOENT;
  slot = free_slot(g_msg);
  if (slot < 0)
    return -ENOSPC;
  struct msq *q = kzalloc(sizeof(*q));
  if (!q)
    return -ENOMEM;
  perm_init(&q->o.perm, (int32_t)key, (int)flg, 0, slot);
  q->o.id = make_id(0, slot);
  list_init(&q->msgs);
  waitq_init(&q->wq);
  q->qbytes = MSGMNB;
  q->ctime = now_sec();
  msqs[slot] = q;
  return q->o.id;
}

static void msq_free(int slot) {
  struct msq *q = msqs[slot];
  list_for_each_safe(p, n, &q->msgs) {
    list_del(p);
    kfree(list_entry(p, struct msg, link));
  }
  msqs[slot] = NULL;
  seqs[0][slot]++;
  wake_up(&q->wq);
  kfree(q);
}

long sys_msgsnd(long id, long umsg, long size, long flg) {
  if (size < 0 || size > MSGMAX)
    return -EINVAL;
  int slot = slot_of(g_msg, (int)id);
  if (slot < 0)
    return slot;
  int64_t type;
  if (copy_from_user(&type, (void *)umsg, 8))
    return -EFAULT;
  if (type < 1)
    return -EINVAL;
  struct msg *m = kmalloc(sizeof(*m) + (size_t)size + 1);
  if (!m)
    return -ENOMEM;
  m->type = type;
  m->len = (size_t)size;
  if (copy_from_user(m->text, (char *)umsg + 8, (size_t)size)) {
    kfree(m);
    return -EFAULT;
  }
  uint64_t f = irq_save();
  for (;;) {
    struct msq *q = msqs[slot];
    if (!q || q->o.id != id) {
      irq_restore(f);
      kfree(m);
      return -EIDRM;
    }
    int e = ipc_perm_ok(&q->o.perm, MAY_WRITE);
    if (e) {
      irq_restore(f);
      kfree(m);
      return e;
    }
    if (q->cbytes + (uint64_t)size <= q->qbytes && q->qnum + 1 <= q->qbytes) {
      list_add_tail(&q->msgs, &m->link);
      q->cbytes += (uint64_t)size;
      q->qnum++;
      q->lspid = cur_proc()->pid;
      q->stime = now_sec();
      wake_up(&q->wq);
      irq_restore(f);
      return 0;
    }
    if (flg & IPC_NOWAIT) {
      irq_restore(f);
      kfree(m);
      return -EAGAIN;
    }
    if (sleep_on(&q->wq, 0, true) == -EINTR) {
      irq_restore(f);
      kfree(m);
      return -EINTR;
    }
  }
}

static bool msg_match(struct msg *m, int64_t type, long flg) {
  if (type == 0)
    return true;
  if (type > 0)
    return (flg & MSG_EXCEPT) ? m->type != type : m->type == type;
  return m->type <= -type;
}

long sys_msgrcv(long id, long umsg, long size, long type, long flg) {
  if (size < 0)
    return -EINVAL;
  int slot = slot_of(g_msg, (int)id);
  if (slot < 0)
    return slot;
  uint64_t f = irq_save();
  for (;;) {
    struct msq *q = msqs[slot];
    if (!q || q->o.id != id) {
      irq_restore(f);
      return -EIDRM;
    }
    int e = ipc_perm_ok(&q->o.perm, MAY_READ);
    if (e) {
      irq_restore(f);
      return e;
    }
    struct msg *best = NULL;
    list_for_each(p, &q->msgs) {
      struct msg *m = list_entry(p, struct msg, link);
      if (!msg_match(m, type, flg))
        continue;
      if (type < 0) {
        if (!best || m->type < best->type)
          best = m;
      } else {
        best = m;
        break;
      }
    }
    if (best) {
      if (best->len > (size_t)size && !(flg & MSG_NOERROR)) {
        irq_restore(f);
        return -E2BIG;
      }
      size_t n = MIN(best->len, (size_t)size);
      list_del(&best->link);
      q->cbytes -= best->len;
      q->qnum--;
      q->lrpid = cur_proc()->pid;
      q->rtime = now_sec();
      wake_up(&q->wq);
      irq_restore(f);
      int r = copy_to_user((void *)umsg, &best->type, 8) || copy_to_user((char *)umsg + 8, best->text, n);
      kfree(best);
      return r ? -EFAULT : (long)n;
    }
    if (flg & IPC_NOWAIT) {
      irq_restore(f);
      return -ENOMSG;
    }
    if (sleep_on(&q->wq, 0, true) == -EINTR) {
      irq_restore(f);
      return -EINTR;
    }
  }
}

struct kmsqid {
  struct kperm perm;
  int64_t stime, rtime, ctime;
  uint64_t cbytes, qnum, qbytes;
  int32_t lspid, lrpid;
  uint64_t unused4, unused5;
};

struct kmsginfo {
  int32_t pool, map, max, mnb, mni, ssz, tql;
  uint16_t seg, pad;
};

static int used_slots(getter g, int *highest) {
  int n = 0;
  *highest = -1;
  for (int i = 0; i < NSLOT; i++)
    if (g(i)) {
      n++;
      *highest = i;
    }
  return n;
}

long sys_msgctl(long id, long cmd, long ubuf) {
  cmd &= ~0x100; /* IPC_64 */
  if (cmd == IPC_INFO || cmd == MSG_INFO) {
    struct kmsginfo mi = {MSGMNB / 1024, 0, MSGMAX, MSGMNB, NSLOT, 16, 0, 0, 0};
    int hi;
    int n = used_slots(g_msg, &hi);
    if (cmd == MSG_INFO) {
      mi.pool = n;
      mi.map = 0;
      mi.tql = 0;
      for (int i = 0; i < NSLOT; i++)
        if (msqs[i]) {
          mi.map += (int32_t)msqs[i]->qnum;
          mi.tql += (int32_t)msqs[i]->cbytes;
        }
    }
    if (copy_to_user((void *)ubuf, &mi, sizeof(mi)))
      return -EFAULT;
    return hi < 0 ? 0 : hi;
  }
  int slot;
  if (cmd == MSG_STAT || cmd == MSG_STAT_ANY) {
    slot = (int)id;
    if (slot < 0 || slot >= NSLOT || !msqs[slot])
      return -EINVAL;
  } else if ((slot = slot_of(g_msg, (int)id)) < 0)
    return slot;
  struct msq *q = msqs[slot];
  switch (cmd) {
  case IPC_STAT:
  case MSG_STAT:
  case MSG_STAT_ANY: {
    if (cmd != MSG_STAT_ANY) {
      int e = ipc_perm_ok(&q->o.perm, MAY_READ);
      if (e)
        return e;
    }
    struct kmsqid d;
    memset(&d, 0, sizeof(d));
    d.perm = q->o.perm;
    d.stime = q->stime;
    d.rtime = q->rtime;
    d.ctime = q->ctime;
    d.cbytes = q->cbytes;
    d.qnum = q->qnum;
    d.qbytes = q->qbytes;
    d.lspid = q->lspid;
    d.lrpid = q->lrpid;
    if (copy_to_user((void *)ubuf, &d, sizeof(d)))
      return -EFAULT;
    return cmd == IPC_STAT ? 0 : q->o.id;
  }
  case IPC_SET: {
    struct kmsqid d;
    if (copy_from_user(&d, (void *)ubuf, sizeof(d)))
      return -EFAULT;
    if (!is_owner(&q->o.perm))
      return -EPERM;
    if (d.qbytes > MSGMNB && !capable())
      return -EPERM;
    int e = perm_set((void *)ubuf, &q->o.perm);
    if (e)
      return e;
    q->qbytes = d.qbytes;
    q->ctime = now_sec();
    wake_up(&q->wq);
    return 0;
  }
  case IPC_RMID:
    if (!is_owner(&q->o.perm))
      return -EPERM;
    msq_free(slot);
    return 0;
  }
  return -EINVAL;
}

/* ---- semaphores ---- */

struct undo {
  struct list link;
  int id;
  int nsems;
  short adj[];
};

/* per process undo lists, kept by pid */
struct undolist {
  struct list link;
  pid_t pid;
  struct list undos;
};
static struct list undolists = LIST_INIT(undolists);

static struct undolist *undo_of(struct proc *p, bool create) {
  list_for_each(q, &undolists) {
    struct undolist *u = list_entry(q, struct undolist, link);
    if (u->pid == p->pid)
      return u;
  }
  if (!create)
    return NULL;
  struct undolist *u = kzalloc(sizeof(*u));
  if (!u)
    return NULL;
  u->pid = p->pid;
  list_init(&u->undos);
  list_add_tail(&undolists, &u->link);
  return u;
}

long sys_semget(long key, long nsems, long flg) {
  int slot = find_key(g_sem, (int32_t)key);
  if (nsems < 0 || nsems > SEMMSL)
    return -EINVAL;
  if (slot >= 0) {
    if ((flg & IPC_CREAT) && (flg & IPC_EXCL))
      return -EEXIST;
    struct semset *s = sems[slot];
    if (nsems > s->nsems)
      return -EINVAL;
    int e = ipc_perm_ok(&s->o.perm, ((int)flg >> 6) & 7 & ~1);
    return e ? e : s->o.id;
  }
  if (!(flg & IPC_CREAT) && key != IPC_PRIVATE)
    return -ENOENT;
  if (nsems == 0)
    return -EINVAL;
  slot = free_slot(g_sem);
  if (slot < 0)
    return -ENOSPC;
  struct semset *s = kzalloc(sizeof(*s));
  if (!s)
    return -ENOMEM;
  s->sem = kzalloc((size_t)nsems * sizeof(*s->sem));
  if (!s->sem) {
    kfree(s);
    return -ENOMEM;
  }
  perm_init(&s->o.perm, (int32_t)key, (int)flg, 1, slot);
  s->o.id = make_id(1, slot);
  s->nsems = (int)nsems;
  s->ctime = now_sec();
  waitq_init(&s->wq);
  sems[slot] = s;
  return s->o.id;
}

static void sem_free(int slot) {
  struct semset *s = sems[slot];
  sems[slot] = NULL;
  seqs[1][slot]++;
  wake_up(&s->wq);
  kfree(s->sem);
  kfree(s);
}

struct sembuf {
  uint16_t num;
  int16_t op, flg;
};

/* can the whole operation set be done now? */
static int sem_try(struct semset *s, struct sembuf *ops, int n, bool apply) {
  for (int i = 0; i < n; i++) {
    struct sem *m = &s->sem[ops[i].num];
    int op = ops[i].op;
    if (op < 0 && m->val + op < 0)
      return 0;
    if (op == 0 && m->val != 0)
      return 0;
    if (op > 0 && m->val + op > SEMVMX)
      return -ERANGE;
  }
  if (apply)
    for (int i = 0; i < n; i++) {
      s->sem[ops[i].num].val += ops[i].op;
      s->sem[ops[i].num].pid = cur_proc()->pid;
    }
  return 1;
}

static long do_semop(long id, long uops, long nops, long uts) {
  if (nops < 1 || nops > SEMOPM)
    return nops < 1 ? -EINVAL : -E2BIG;
  int slot = slot_of(g_sem, (int)id);
  if (slot < 0)
    return slot;
  struct sembuf *ops = kmalloc((size_t)nops * sizeof(*ops));
  if (!ops)
    return -ENOMEM;
  if (copy_from_user(ops, (void *)uops, (size_t)nops * sizeof(*ops))) {
    kfree(ops);
    return -EFAULT;
  }
  uint64_t deadline = 0;
  if (uts) {
    struct timespec64 ts;
    if (copy_from_user(&ts, (void *)uts, sizeof(ts))) {
      kfree(ops);
      return -EFAULT;
    }
    if (ts.sec < 0 || ts.nsec < 0 || ts.nsec >= 1000000000) {
      kfree(ops);
      return -EINVAL;
    }
    deadline = sat_add(monotonic_ns(), sat_add(ts_to_ns(ts.sec, ts.nsec), 1));
  }
  long r;
  uint64_t f = irq_save();
  for (;;) {
    struct semset *s = sems[slot];
    if (!s || s->o.id != id) {
      r = -EIDRM;
      break;
    }
    bool alter = false;
    for (int i = 0; i < nops; i++) {
      if (ops[i].num >= s->nsems) {
        r = -EFBIG;
        goto done;
      }
      if (ops[i].op)
        alter = true;
    }
    int e = ipc_perm_ok(&s->o.perm, alter ? MAY_WRITE : MAY_READ);
    if (e) {
      r = e;
      break;
    }
    int t = sem_try(s, ops, (int)nops, true);
    if (t < 0) {
      r = t;
      break;
    }
    if (t == 1) {
      s->otime = now_sec();
      /* undo records */
      for (int i = 0; i < nops; i++) {
        if (!(ops[i].flg & SEM_UNDO) || !ops[i].op)
          continue;
        struct undolist *ul = undo_of(cur_proc(), true);
        struct undo *u = NULL;
        list_for_each(q, &ul->undos) {
          struct undo *x = list_entry(q, struct undo, link);
          if (x->id == id)
            u = x;
        }
        if (!u) {
          u = kzalloc(sizeof(*u) + (size_t)s->nsems * sizeof(short));
          u->id = (int)id;
          u->nsems = s->nsems;
          list_add_tail(&ul->undos, &u->link);
        }
        u->adj[ops[i].num] = (short)(u->adj[ops[i].num] - ops[i].op);
      }
      wake_up(&s->wq);
      r = 0;
      break;
    }
    bool nowait = false;
    for (int i = 0; i < nops; i++)
      if (ops[i].flg & IPC_NOWAIT)
        nowait = true;
    if (nowait) {
      r = -EAGAIN;
      break;
    }
    /* count the waiters for GETNCNT/GETZCNT */
    current->fkey1 = (uint64_t)id;
    current->fkey2 = ops[0].num | (uint64_t)(ops[0].op == 0) << 32;
    current->fbitset = 0x5e5;
    e = sleep_on(&s->wq, deadline, true);
    current->fbitset = 0;
    if (e == -ETIMEDOUT) {
      r = -EAGAIN;
      break;
    }
    if (e == -EINTR) {
      r = -EINTR;
      break;
    }
  }
done:
  irq_restore(f);
  kfree(ops);
  return r;
}

long sys_semop(long id, long uops, long nops) { return do_semop(id, uops, nops, 0); }
long sys_semtimedop(long id, long uops, long nops, long uts) { return do_semop(id, uops, nops, uts); }

/* waiters on semaphore num of id: for zero (z) or for an increase */
static int count_waiters(int id, int num, bool z) {
  int n = 0;
  list_for_each(p, &all_tasks) {
    struct task *t = list_entry(p, struct task, all_link);
    if (t->fbitset == 0x5e5 && t->fkey1 == (uint64_t)id && (int)(t->fkey2 & 0xffff) == num &&
        ((t->fkey2 >> 32) != 0) == z)
      n++;
  }
  return n;
}

/* x86-64: padding words after each time */
struct ksemid {
  struct kperm perm;
  int64_t otime;
  uint64_t unused1;
  int64_t ctime;
  uint64_t unused2;
  uint64_t nsems;
  uint64_t unused3, unused4;
};

struct kseminfo {
  int32_t map, mni, mns, mnu, msl, opm, ume, usz, vmx, aem;
};

long sys_semctl(long id, long num, long cmd, long arg) {
  cmd &= ~0x100;
  if (cmd == IPC_INFO || cmd == SEM_INFO) {
    struct kseminfo si = {NSLOT, NSLOT, NSLOT * SEMMSL, NSLOT, SEMMSL, SEMOPM, SEMOPM, 20, SEMVMX, 0};
    int hi;
    int n = used_slots(g_sem, &hi);
    if (cmd == SEM_INFO) {
      si.usz = n;
      si.aem = 0;
      for (int i = 0; i < NSLOT; i++)
        if (sems[i])
          si.aem += sems[i]->nsems;
    }
    if (copy_to_user((void *)arg, &si, sizeof(si)))
      return -EFAULT;
    return hi < 0 ? 0 : hi;
  }
  int slot;
  if (cmd == SEM_STAT || cmd == SEM_STAT_ANY) {
    slot = (int)id;
    if (slot < 0 || slot >= NSLOT || !sems[slot])
      return -EINVAL;
  } else if ((slot = slot_of(g_sem, (int)id)) < 0)
    return slot;
  struct semset *s = sems[slot];
  int e;
  switch (cmd) {
  case IPC_STAT:
  case SEM_STAT:
  case SEM_STAT_ANY: {
    if (cmd != SEM_STAT_ANY && (e = ipc_perm_ok(&s->o.perm, MAY_READ)))
      return e;
    struct ksemid d;
    memset(&d, 0, sizeof(d));
    d.perm = s->o.perm;
    d.otime = s->otime;
    d.ctime = s->ctime;
    d.nsems = (uint64_t)s->nsems;
    if (copy_to_user((void *)arg, &d, sizeof(d)))
      return -EFAULT;
    return cmd == IPC_STAT ? 0 : s->o.id;
  }
  case IPC_SET:
    e = perm_set((void *)arg, &s->o.perm);
    if (!e)
      s->ctime = now_sec();
    return e;
  case IPC_RMID:
    if (!is_owner(&s->o.perm))
      return -EPERM;
    sem_free(slot);
    return 0;
  case GETVAL:
  case GETPID:
  case GETNCNT:
  case GETZCNT:
    if (num < 0 || num >= s->nsems)
      return -EINVAL;
    if ((e = ipc_perm_ok(&s->o.perm, MAY_READ)))
      return e;
    if (cmd == GETVAL)
      return s->sem[num].val;
    if (cmd == GETPID)
      return s->sem[num].pid;
    return count_waiters((int)id, (int)num, cmd == GETZCNT);
  case GETALL: {
    if ((e = ipc_perm_ok(&s->o.perm, MAY_READ)))
      return e;
    for (int i = 0; i < s->nsems; i++) {
      uint16_t v = (uint16_t)s->sem[i].val;
      if (copy_to_user((uint16_t *)arg + i, &v, 2))
        return -EFAULT;
    }
    return 0;
  }
  case SETVAL:
    if (num < 0 || num >= s->nsems)
      return -EINVAL;
    if ((e = ipc_perm_ok(&s->o.perm, MAY_WRITE)))
      return e;
    if ((int)arg < 0 || (int)arg > SEMVMX)
      return -ERANGE;
    s->sem[num].val = (int)arg;
    s->sem[num].pid = cur_proc()->pid;
    s->ctime = now_sec();
    /* undo adjustments for it are cleared in every process */
    list_for_each(q, &undolists) {
      struct undolist *ul = list_entry(q, struct undolist, link);
      list_for_each(r, &ul->undos) {
        struct undo *u = list_entry(r, struct undo, link);
        if (u->id == id && num < u->nsems)
          u->adj[num] = 0;
      }
    }
    wake_up(&s->wq);
    return 0;
  case SETALL: {
    if ((e = ipc_perm_ok(&s->o.perm, MAY_WRITE)))
      return e;
    uint16_t *v = kmalloc((size_t)s->nsems * 2 + 2);
    if (copy_from_user(v, (void *)arg, (size_t)s->nsems * 2)) {
      kfree(v);
      return -EFAULT;
    }
    for (int i = 0; i < s->nsems; i++)
      if (v[i] > SEMVMX) {
        kfree(v);
        return -ERANGE;
      }
    for (int i = 0; i < s->nsems; i++) {
      s->sem[i].val = v[i];
      s->sem[i].pid = cur_proc()->pid;
    }
    kfree(v);
    list_for_each(q, &undolists) {
      struct undolist *ul = list_entry(q, struct undolist, link);
      list_for_each(r, &ul->undos) {
        struct undo *u = list_entry(r, struct undo, link);
        if (u->id == id)
          memset(u->adj, 0, (size_t)u->nsems * sizeof(short));
      }
    }
    s->ctime = now_sec();
    wake_up(&s->wq);
    return 0;
  }
  }
  return -EINVAL;
}

/* at exit: apply SEM_UNDO adjustments */
void sem_exit(struct proc *p) {
  struct undolist *ul = undo_of(p, false);
  if (!ul)
    return;
  list_for_each_safe(q, n, &ul->undos) {
    struct undo *u = list_entry(q, struct undo, link);
    int slot = u->id % 32768;
    struct semset *s = slot < NSLOT ? sems[slot] : NULL;
    if (s && s->o.id == u->id) {
      for (int i = 0; i < u->nsems && i < s->nsems; i++) {
        int v = s->sem[i].val + u->adj[i];
        s->sem[i].val = v < 0 ? 0 : v > SEMVMX ? SEMVMX : v;
        if (u->adj[i])
          s->sem[i].pid = p->pid;
      }
      wake_up(&s->wq);
    }
    list_del(q);
    kfree(u);
  }
  list_del(&ul->link);
  kfree(ul);
}

/* ---- shared memory ---- */

long sys_shmget(long key, long size, long flg) {
  int slot = find_key(g_shm, (int32_t)key);
  if (slot >= 0) {
    if ((flg & IPC_CREAT) && (flg & IPC_EXCL))
      return -EEXIST;
    struct shmseg *s = shms[slot];
    if ((uint64_t)size > s->size)
      return -EINVAL;
    int e = ipc_perm_ok(&s->o.perm, ((int)flg >> 6) & 7 & ~1);
    return e ? e : s->o.id;
  }
  if (!(flg & IPC_CREAT) && key != IPC_PRIVATE)
    return -ENOENT;
  if (size < SHMMIN || (uint64_t)size > SHMMAX)
    return -EINVAL;
  slot = free_slot(g_shm);
  if (slot < 0)
    return -ENOSPC;
  struct shmseg *s = kzalloc(sizeof(*s));
  if (!s)
    return -ENOMEM;
  s->obj = shmobj_new(PAGE_UP((uint64_t)size) >> 12);
  if (!s->obj) {
    kfree(s);
    return -ENOMEM;
  }
  perm_init(&s->o.perm, (int32_t)key, (int)flg, 2, slot);
  s->o.id = make_id(2, slot);
  s->size = (uint64_t)size;
  s->cpid = cur_proc()->pid;
  s->ctime = now_sec();
  shms[slot] = s;
  return s->o.id;
}

static void shm_destroy(int slot) {
  struct shmseg *s = shms[slot];
  shms[slot] = NULL;
  seqs[2][slot]++;
  shmobj_put(s->obj);
  kfree(s);
}

static struct shmseg *seg_by_id(int id) {
  int slot = slot_of(g_shm, id);
  return slot < 0 ? NULL : shms[slot];
}

void shm_attach_vma(int id) {
  struct shmseg *s = seg_by_id(id);
  if (s) {
    s->nattch++;
    s->atime = now_sec();
    s->lpid = cur_proc()->pid;
  }
}

void shm_detach_vma(int id) {
  int slot = slot_of(g_shm, id);
  if (slot < 0)
    return;
  struct shmseg *s = shms[slot];
  if (s->nattch)
    s->nattch--;
  s->dtime = now_sec();
  if (current && current->proc)
    s->lpid = cur_proc()->pid;
  if (s->dest && s->nattch == 0)
    shm_destroy(slot);
}

long sys_shmat(long id, long addr, long flg) {
  struct shmseg *s = seg_by_id((int)id);
  if (!s)
    return -EINVAL;
  int e = ipc_perm_ok(&s->o.perm, (flg & SHM_RDONLY) ? MAY_READ : MAY_READ | MAY_WRITE);
  if (e)
    return e;
  struct mm *mm = cur_proc()->mm;
  uint64_t len = PAGE_UP(s->size);
  uint64_t a = (uint64_t)addr;
  if (a) {
    if (a & 0xfff) {
      if (!(flg & SHM_RND))
        return -EINVAL;
      a &= ~0xfffUL;
    }
    if (a + len > USER_TOP)
      return -EINVAL;
    if (flg & SHM_REMAP)
      vma_unmap(mm, a, a + len);
    else {
      list_for_each(p, &mm->vmas) {
        struct vma *v = list_entry(p, struct vma, link);
        if (v->start < a + len && v->end > a)
          return -EINVAL;
      }
    }
  } else {
    a = vma_free_area(mm, len, 0);
    if (!a)
      return -ENOMEM;
  }
  unsigned vf = VM_READ | VM_SHARED | ((flg & SHM_RDONLY) ? 0 : VM_WRITE) | ((flg & SHM_EXEC) ? VM_EXEC : 0);
  struct vma *v = vma_new(mm, a, a + len, vf);
  if (!v)
    return -ENOMEM;
  v->shm = s->obj;
  shmobj_get(s->obj);
  v->shm_off = 0;
  v->shmid = (int)id;
  static unsigned attaches;
  v->attach = ++attaches;
  shm_attach_vma((int)id);
  return (long)a;
}

long sys_shmdt(long addr) {
  struct mm *mm = cur_proc()->mm;
  if (addr & 0xfff)
    return -EINVAL;
  struct vma *v = vma_find(mm, (uint64_t)addr);
  if (!v || v->start != (uint64_t)addr || v->shmid < 0)
    return -EINVAL;
  int id = v->shmid;
  uint64_t start = v->start, end = v->end;
  /* the pieces of this attachment that follow (mprotect splits) */
  list_for_each(p, &mm->vmas) {
    struct vma *w = list_entry(p, struct vma, link);
    if (w->start == end && w->shmid == id && w->attach == v->attach)
      end = w->end;
  }
  return vma_unmap(mm, start, end);
}

struct kshmid {
  struct kperm perm;
  uint64_t segsz;
  int64_t atime, dtime, ctime;
  int32_t cpid, lpid;
  uint64_t nattch, unused4, unused5;
};

struct kshminfo {
  uint64_t max, min, mni, seg, all, unused[4];
};

struct kshm_info {
  int32_t used_ids, pad;
  uint64_t tot, rss, swp, swap_attempts, swap_successes;
};

long sys_shmctl(long id, long cmd, long ubuf) {
  cmd &= ~0x100;
  if (cmd == IPC_INFO) {
    struct kshminfo si = {SHMMAX, SHMMIN, NSLOT, NSLOT, 1UL << 30, {0}};
    int hi;
    used_slots(g_shm, &hi);
    if (copy_to_user((void *)ubuf, &si, sizeof(si)))
      return -EFAULT;
    return hi < 0 ? 0 : hi;
  }
  if (cmd == SHM_INFO) {
    struct kshm_info si;
    memset(&si, 0, sizeof(si));
    int hi;
    si.used_ids = used_slots(g_shm, &hi);
    for (int i = 0; i < NSLOT; i++)
      if (shms[i])
        si.tot += PAGE_UP(shms[i]->size) >> 12;
    if (copy_to_user((void *)ubuf, &si, sizeof(si)))
      return -EFAULT;
    return hi < 0 ? 0 : hi;
  }
  int slot;
  if (cmd == SHM_STAT || cmd == SHM_STAT_ANY) {
    slot = (int)id;
    if (slot < 0 || slot >= NSLOT || !shms[slot])
      return -EINVAL;
  } else if ((slot = slot_of(g_shm, (int)id)) < 0)
    return slot;
  struct shmseg *s = shms[slot];
  int e;
  switch (cmd) {
  case IPC_STAT:
  case SHM_STAT:
  case SHM_STAT_ANY: {
    if (cmd != SHM_STAT_ANY && (e = ipc_perm_ok(&s->o.perm, MAY_READ)))
      return e;
    struct kshmid d;
    memset(&d, 0, sizeof(d));
    d.perm = s->o.perm;
    d.segsz = s->size;
    d.atime = s->atime;
    d.dtime = s->dtime;
    d.ctime = s->ctime;
    d.cpid = s->cpid;
    d.lpid = s->lpid;
    d.nattch = s->nattch;
    if (copy_to_user((void *)ubuf, &d, sizeof(d)))
      return -EFAULT;
    return cmd == IPC_STAT ? 0 : s->o.id;
  }
  case IPC_SET:
    e = perm_set((void *)ubuf, &s->o.perm);
    if (!e)
      s->ctime = now_sec();
    return e;
  case IPC_RMID:
    if (!is_owner(&s->o.perm))
      return -EPERM;
    if (s->nattch == 0) {
      shm_destroy(slot);
      return 0;
    }
    s->dest = true;
    s->o.perm.mode |= SHM_DEST;
    s->o.perm.key = IPC_PRIVATE;
    return 0;
  case SHM_LOCK:
  case SHM_UNLOCK:
    return capable() ? 0 : -EPERM;
  }
  return -EINVAL;
}

/* /proc/sysvipc/{msg,shm,sem} */
void ipc_proc_text(int which, void (*emit)(void *ctx, const char *line), void *ctx) {
  char line[400];
  for (int i = 0; i < NSLOT; i++) {
    if (which == 0 && msqs[i]) {
      struct msq *q = msqs[i];
      struct kperm *p = &q->o.perm;
      snprintf(line, sizeof(line), "%10d %10d  %4o  %10lu %10lu %5d %5d %5u %5u %5u %5u %10ld %10ld %10ld\n",
               p->key, q->o.id, p->mode & 0777, q->cbytes, q->qnum, q->lspid, q->lrpid, p->uid, p->gid,
               p->cuid, p->cgid, q->stime, q->rtime, q->ctime);
      emit(ctx, line);
    } else if (which == 1 && shms[i]) {
      struct shmseg *s = shms[i];
      struct kperm *p = &s->o.perm;
      snprintf(line, sizeof(line),
               "%10d %10d  %4o %21lu %5d %5d  %5lu %5u %5u %5u %5u %10ld %10ld %10ld %21lu %21lu\n", p->key,
               s->o.id, p->mode & 01777, s->size, s->cpid, s->lpid, s->nattch, p->uid, p->gid, p->cuid,
               p->cgid, s->atime, s->dtime, s->ctime, 0UL, 0UL);
      emit(ctx, line);
    } else if (which == 2 && sems[i]) {
      struct semset *s = sems[i];
      struct kperm *p = &s->o.perm;
      snprintf(line, sizeof(line), "%10d %10d  %4o %10d %5u %5u %5u %5u %10ld %10ld\n", p->key, s->o.id,
               p->mode & 0777, s->nsems, p->uid, p->gid, p->cuid, p->cgid, s->otime, s->ctime);
      emit(ctx, line);
    }
  }
}

/* procfs.c - /proc: per-process directories (stat, status, cmdline,
   environ, comm, wchan, exe, cwd, root, fd/, maps, mounts, mountinfo) and
   system files (mounts, meminfo, stat, uptime, loadavg, version, cpuinfo,
   filesystems, self, thread-self, net/dev, sys/kernel/..., sysvipc/...).
   Contents are made when a file is opened. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <machine/cpu.h>

#define PROC_MAGIC 0x9fa0

enum {
  R_ROOT = 1, R_MOUNTS, R_MEMINFO, R_UPTIME, R_VERSION, R_STAT, R_CPUINFO, R_FILESYSTEMS,
  R_LOADAVG, R_SELF, R_THREADSELF, R_NET, R_NETDEV, R_NETPNP, R_SYS, R_SYSKERNEL, R_MSGMNB, R_SYSVIPC,
  R_IPCMSG, R_IPCSHM, R_IPCSEM, R_CMDLINE, R_PIDMAX, R_HOSTNAME, R_OSTYPE, R_OSRELEASE, R_NGROUPS,
  R_LAST
};

enum { P_DIR = 1, P_STAT, P_STATUS, P_CMDLINE, P_EXE, P_CWD, P_ROOT, P_FDDIR, P_MAPS, P_MOUNTS,
       P_MOUNTINFO, P_COMM, P_WCHAN, P_ENVIRON, P_TASKDIR, P_FD = 0x800 };
/* /proc/PID/task/TID/...: inode TBIT | TID << 12 | sub */
#define TBIT (1UL << 40)

struct rent {
  const char *name;
  int ino, parent;
  mode_t mode;
};

static const struct rent rents[] = {
    {"mounts", R_MOUNTS, R_ROOT, S_IFLNK | 0777},
    {"meminfo", R_MEMINFO, R_ROOT, S_IFREG | 0444},
    {"uptime", R_UPTIME, R_ROOT, S_IFREG | 0444},
    {"version", R_VERSION, R_ROOT, S_IFREG | 0444},
    {"stat", R_STAT, R_ROOT, S_IFREG | 0444},
    {"cpuinfo", R_CPUINFO, R_ROOT, S_IFREG | 0444},
    {"filesystems", R_FILESYSTEMS, R_ROOT, S_IFREG | 0444},
    {"loadavg", R_LOADAVG, R_ROOT, S_IFREG | 0444},
    {"self", R_SELF, R_ROOT, S_IFLNK | 0777},
    {"thread-self", R_THREADSELF, R_ROOT, S_IFLNK | 0777},
    {"net", R_NET, R_ROOT, S_IFDIR | 0555},
    {"dev", R_NETDEV, R_NET, S_IFREG | 0444},
    {"pnp", R_NETPNP, R_NET, S_IFREG | 0444},
    {"sys", R_SYS, R_ROOT, S_IFDIR | 0555},
    {"kernel", R_SYSKERNEL, R_SYS, S_IFDIR | 0555},
    {"msgmnb", R_MSGMNB, R_SYSKERNEL, S_IFREG | 0444},
    {"pid_max", R_PIDMAX, R_SYSKERNEL, S_IFREG | 0444},
    {"hostname", R_HOSTNAME, R_SYSKERNEL, S_IFREG | 0444},
    {"ostype", R_OSTYPE, R_SYSKERNEL, S_IFREG | 0444},
    {"osrelease", R_OSRELEASE, R_SYSKERNEL, S_IFREG | 0444},
    {"ngroups_max", R_NGROUPS, R_SYSKERNEL, S_IFREG | 0444},
    {"sysvipc", R_SYSVIPC, R_ROOT, S_IFDIR | 0555},
    {"msg", R_IPCMSG, R_SYSVIPC, S_IFREG | 0444},
    {"shm", R_IPCSHM, R_SYSVIPC, S_IFREG | 0444},
    {"sem", R_IPCSEM, R_SYSVIPC, S_IFREG | 0444},
    {"cmdline", R_CMDLINE, R_ROOT, S_IFREG | 0444},
};

static const struct {
  const char *name;
  int sub;
  mode_t mode;
} pents[] = {
    {"stat", P_STAT, S_IFREG | 0444},       {"status", P_STATUS, S_IFREG | 0444},
    {"cmdline", P_CMDLINE, S_IFREG | 0444}, {"environ", P_ENVIRON, S_IFREG | 0400},
    {"comm", P_COMM, S_IFREG | 0644},       {"wchan", P_WCHAN, S_IFREG | 0444},
    {"exe", P_EXE, S_IFLNK | 0777},         {"cwd", P_CWD, S_IFLNK | 0777},
    {"root", P_ROOT, S_IFLNK | 0777},       {"fd", P_FDDIR, S_IFDIR | 0500},
    {"maps", P_MAPS, S_IFREG | 0444},       {"mounts", P_MOUNTS, S_IFREG | 0444},
    {"mountinfo", P_MOUNTINFO, S_IFREG | 0444},
    {"task", P_TASKDIR, S_IFDIR | 0555},
};

static const struct inode_ops proc_dir_iops, proc_link_iops, proc_magic_iops, proc_file_iops;
static const struct file_ops proc_dir_fops, proc_file_fops;
static const struct super_ops proc_sops;
extern char kernel_cmdline[];
extern const char *uts_release, *uts_version, *uts_sysname;
extern char uts_nodename[65];

#define PINO(pid, sub) ((ino_t)(pid) << 12 | (sub))

static struct proc *proc_of(pid_t pid) {
  struct proc *p = find_proc(pid);
  if (!p || p->state == P_DEAD) {
    struct task *t = find_task(pid);
    p = t ? t->proc : NULL;
  }
  return p && p->state != P_DEAD ? p : NULL;
}

static int proc_read_inode(struct inode *i) {
  ino_t ino = i->ino;
  i->uid = i->gid = 0;
  uint64_t ns = realtime_ns();
  i->atime.sec = i->mtime.sec = i->ctime.sec = (int64_t)(ns / 1000000000UL);
  i->nlink = 1;
  if (ino < R_LAST) {
    if (ino == R_ROOT) {
      i->mode = S_IFDIR | 0555;
      i->nlink = 3;
    } else {
      size_t k;
      for (k = 0; k < ARRAY_SIZE(rents) && rents[k].ino != (int)ino; k++)
        ;
      if (k == ARRAY_SIZE(rents))
        return -ENOENT;
      i->mode = rents[k].mode;
    }
  } else if (ino & TBIT) {
    struct task *t = find_task((pid_t)((ino & ~TBIT) >> 12));
    int sub = (int)(ino & 0xfff);
    if (!t || !t->proc)
      return -ENOENT;
    i->uid = t->proc->cred.euid;
    i->gid = t->proc->cred.egid;
    i->mode = sub == P_DIR ? S_IFDIR | 0555 : S_IFREG | 0444;
  } else {
    pid_t pid = (pid_t)(ino >> 12);
    int sub = (int)(ino & 0xfff);
    struct proc *p = proc_of(pid);
    if (!p)
      return -ENOENT;
    i->uid = p->cred.euid;
    i->gid = p->cred.egid;
    if (sub == P_DIR)
      i->mode = S_IFDIR | 0555;
    else if (sub & P_FD) {
      int fd = sub & 0x7ff;
      if (fd >= p->nfd || !p->fd || !p->fd[fd])
        return -ENOENT;
      i->mode = S_IFLNK | 0700;
    } else {
      size_t k;
      for (k = 0; k < ARRAY_SIZE(pents) && pents[k].sub != sub; k++)
        ;
      if (k == ARRAY_SIZE(pents))
        return -ENOENT;
      i->mode = pents[k].mode;
    }
  }
  if (S_ISDIR(i->mode)) {
    i->iops = &proc_dir_iops;
    i->fops = &proc_dir_fops;
  } else if (S_ISLNK(i->mode))
    i->iops = ino < R_LAST ? &proc_link_iops : &proc_magic_iops;
  else {
    i->iops = &proc_file_iops;
    i->fops = &proc_file_fops;
  }
  return 0;
}

static ino_t parent_of(ino_t ino) {
  if (ino & TBIT) {
    struct task *t = find_task((pid_t)((ino & ~TBIT) >> 12));
    pid_t pid = t && t->proc ? t->proc->pid : 1;
    if ((ino & 0xfff) == P_DIR)
      return PINO(pid, P_TASKDIR);
    return ino & ~0xfffUL ? (ino & ~0xfffUL) | P_DIR : R_ROOT;
  }
  if (ino < R_LAST) {
    for (size_t k = 0; k < ARRAY_SIZE(rents); k++)
      if (rents[k].ino == (int)ino)
        return (ino_t)rents[k].parent;
    return R_ROOT;
  }
  int sub = (int)(ino & 0xfff);
  pid_t pid = (pid_t)(ino >> 12);
  if (sub == P_DIR)
    return R_ROOT;
  if (sub == P_TASKDIR)
    return PINO(pid, P_DIR);
  if (sub & P_FD)
    return PINO(pid, P_FDDIR);
  return PINO(pid, P_DIR);
}

static struct inode *proc_lookup(struct inode *dir, const char *name, size_t len) {
  if (len == 2 && name[0] == '.' && name[1] == '.')
    return iget(dir->sb, parent_of(dir->ino));
  ino_t d = dir->ino;
  if (d < R_LAST) {
    for (size_t k = 0; k < ARRAY_SIZE(rents); k++)
      if (rents[k].parent == (int)d && strlen(rents[k].name) == len && !memcmp(rents[k].name, name, len))
        return iget(dir->sb, (ino_t)rents[k].ino);
    if (d == R_ROOT && len < 8) {
      long pid = 0;
      for (size_t i = 0; i < len; i++) {
        if (name[i] < '0' || name[i] > '9')
          return ERR_PTR(-ENOENT);
        pid = pid * 10 + (name[i] - '0');
      }
      if (pid > 0 && proc_of((pid_t)pid))
        return iget(dir->sb, PINO(pid, P_DIR));
    }
    return ERR_PTR(-ENOENT);
  }
  if (d & TBIT) {
    static const struct { const char *n; int sub; } te[] = {{"stat", P_STAT}, {"status", P_STATUS}, {"comm", P_COMM}};
    for (size_t k = 0; k < ARRAY_SIZE(te); k++)
      if (strlen(te[k].n) == len && !memcmp(te[k].n, name, len))
        return iget(dir->sb, (d & ~0xfffUL) | (ino_t)te[k].sub);
    return ERR_PTR(-ENOENT);
  }
  pid_t pid = (pid_t)(d >> 12);
  int sub = (int)(d & 0xfff);
  if (sub == P_TASKDIR && len < 8) {
    long tid = 0;
    for (size_t i = 0; i < len; i++) {
      if (name[i] < '0' || name[i] > '9')
        return ERR_PTR(-ENOENT);
      tid = tid * 10 + (name[i] - '0');
    }
    struct task *t = find_task((pid_t)tid);
    if (!t || !t->proc || t->proc->pid != pid)
      return ERR_PTR(-ENOENT);
    return iget(dir->sb, TBIT | (ino_t)tid << 12 | P_DIR);
  }
  if (sub == P_DIR) {
    for (size_t k = 0; k < ARRAY_SIZE(pents); k++)
      if (strlen(pents[k].name) == len && !memcmp(pents[k].name, name, len))
        return iget(dir->sb, PINO(pid, pents[k].sub));
  } else if (sub == P_FDDIR && len < 5) {
    long fd = 0;
    for (size_t i = 0; i < len; i++) {
      if (name[i] < '0' || name[i] > '9')
        return ERR_PTR(-ENOENT);
      fd = fd * 10 + (name[i] - '0');
    }
    if (fd < 0x800)
      return iget(dir->sb, PINO(pid, P_FD | fd));
  }
  return ERR_PTR(-ENOENT);
}

static int proc_readdir(struct file *f, struct dir_ctx *ctx) {
  ino_t d = f->ino->ino;
  if (f->pos == 0) {
    if (!ctx->fill(ctx, ".", 1, d, DT_DIR, 1))
      return 0;
    f->pos = 1;
  }
  if (f->pos == 1) {
    if (!ctx->fill(ctx, "..", 2, parent_of(d), DT_DIR, 2))
      return 0;
    f->pos = 2;
  }
  if (d < R_LAST) {
    off_t k = 2;
    for (size_t i = 0; i < ARRAY_SIZE(rents); i++) {
      if (rents[i].parent != (int)d)
        continue;
      if (k++ < f->pos)
        continue;
      if (!ctx->fill(ctx, rents[i].name, strlen(rents[i].name), (ino_t)rents[i].ino, IFTODT(rents[i].mode), k))
        return 0;
      f->pos = k;
    }
    if (d != R_ROOT)
      return 0;
    /* processes, by increasing pid: position 100000 + pid */
    for (;;) {
      pid_t last = f->pos >= 100000 ? (pid_t)(f->pos - 100000) : 0;
      pid_t next = 0;
      list_for_each(p, &all_procs) {
        struct proc *pr = list_entry(p, struct proc, all_link);
        if (pr->state != P_DEAD && pr->pid > last && (!next || pr->pid < next))
          next = pr->pid;
      }
      if (!next)
        return 0;
      char n[12];
      int len = snprintf(n, sizeof(n), "%d", next);
      if (!ctx->fill(ctx, n, (size_t)len, PINO(next, P_DIR), DT_DIR, 100000 + next))
        return 0;
      f->pos = 100000 + next;
    }
  }
  if (d & TBIT) {
    static const char *te[] = {"stat", "status", "comm"};
    static const int ts[] = {P_STAT, P_STATUS, P_COMM};
    for (off_t k = f->pos; k - 2 < 3; k++) {
      if (!ctx->fill(ctx, te[k - 2], strlen(te[k - 2]), (d & ~0xfffUL) | (ino_t)ts[k - 2], DT_REG, k + 1))
        return 0;
      f->pos = k + 1;
    }
    return 0;
  }
  pid_t pid = (pid_t)(d >> 12);
  int sub = (int)(d & 0xfff);
  struct proc *p = proc_of(pid);
  if (!p)
    return 0;
  if (sub == P_TASKDIR) {
    for (;;) {
      pid_t last = f->pos >= 100000 ? (pid_t)(f->pos - 100000) : 0;
      pid_t next = 0;
      list_for_each(q, &p->threads) {
        struct task *t = list_entry(q, struct task, thread_link);
        if (t->tid > last && (!next || t->tid < next))
          next = t->tid;
      }
      if (!next)
        return 0;
      char n[12];
      int len = snprintf(n, sizeof(n), "%d", next);
      if (!ctx->fill(ctx, n, (size_t)len, TBIT | (ino_t)next << 12 | P_DIR, DT_DIR, 100000 + next))
        return 0;
      f->pos = 100000 + next;
    }
  }
  if (sub == P_DIR) {
    off_t k = 2;
    for (size_t i = 0; i < ARRAY_SIZE(pents); i++) {
      if (k++ < f->pos)
        continue;
      if (!ctx->fill(ctx, pents[i].name, strlen(pents[i].name), PINO(pid, pents[i].sub), IFTODT(pents[i].mode), k))
        return 0;
      f->pos = k;
    }
  } else if (sub == P_FDDIR) {
    for (int fd = (int)f->pos - 2; fd < p->nfd && fd < 0x800; fd++) {
      if (!p->fd[fd])
        continue;
      char n[12];
      int len = snprintf(n, sizeof(n), "%d", fd);
      if (!ctx->fill(ctx, n, (size_t)len, PINO(pid, P_FD | fd), DT_LNK, fd + 3))
        return 0;
      f->pos = fd + 3;
    }
    f->pos = 2 + 0x800;
  }
  return 0;
}

static ssize_t proc_readlink(struct inode *i, char *buf, size_t n) {
  char tmp[4096];
  ino_t ino = i->ino;
  tmp[0] = 0;
  if (ino == R_SELF)
    snprintf(tmp, sizeof(tmp), "%d", cur_proc()->pid);
  else if (ino == R_THREADSELF)
    snprintf(tmp, sizeof(tmp), "%d/task/%d", cur_proc()->pid, current->tid);
  else if (ino == R_MOUNTS)
    strcpy(tmp, "self/mounts");
  else {
    struct proc *p = proc_of((pid_t)(ino >> 12));
    int sub = (int)(ino & 0xfff);
    if (!p)
      return -ENOENT;
    if (sub == P_EXE) {
      if (!p->exe)
        return -ENOENT;
      snprintf(tmp, sizeof(tmp), "%s", p->exe);
    } else if (sub == P_CWD || sub == P_ROOT) {
      struct path *pp = sub == P_CWD ? &p->cwd : &p->root;
      if (!pp->ino || path_string(pp, tmp, sizeof(tmp)))
        return -ENOENT;
    } else if (sub & P_FD) {
      int fd = sub & 0x7ff;
      if (fd >= p->nfd || !p->fd[fd])
        return -ENOENT;
      struct file *f = p->fd[fd];
      if (f->ino && f->ino->pipe && !f->ino->sb->type->needs_dev && !f->mnt)
        snprintf(tmp, sizeof(tmp), "pipe:[%lu]", f->ino->ino);
      else if (f->ino && S_ISSOCK(f->ino->mode) && !f->mnt)
        snprintf(tmp, sizeof(tmp), "socket:[%lu]", f->ino->ino);
      else if (f->path)
        snprintf(tmp, sizeof(tmp), "%s", f->path);
      else
        snprintf(tmp, sizeof(tmp), "anon_inode:[%lu]", f->ino ? f->ino->ino : 0);
    }
  }
  size_t l = MIN(strlen(tmp), n);
  memcpy(buf, tmp, l);
  return (ssize_t)l;
}

/* ---- generated files ---- */

struct sbuf {
  char *b;
  size_t len, cap;
};

static void sb_printf(struct sbuf *s, const char *fmt, ...) {
  char tmp[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n > (int)sizeof(tmp) - 1)
    n = sizeof(tmp) - 1;
  if (s->len + (size_t)n + 1 > s->cap) {
    size_t nc = s->cap ? s->cap * 2 : 4096;
    while (nc < s->len + (size_t)n + 1)
      nc *= 2;
    char *nb = krealloc(s->b, s->cap, nc);
    if (!nb)
      return;
    s->b = nb;
    s->cap = nc;
  }
  memcpy(s->b + s->len, tmp, (size_t)n);
  s->len += (size_t)n;
}

static void sb_bytes(struct sbuf *s, const char *p, size_t n) {
  if (s->len + n + 1 > s->cap) {
    size_t nc = s->cap ? s->cap * 2 : 4096;
    while (nc < s->len + n + 1)
      nc *= 2;
    char *nb = krealloc(s->b, s->cap, nc);
    if (!nb)
      return;
    s->b = nb;
    s->cap = nc;
  }
  memcpy(s->b + s->len, p, n);
  s->len += n;
}

static char state_char(struct proc *p) {
  if (p->state == P_ZOMBIE)
    return 'Z';
  if (p->stopped)
    return 'T';
  bool sleeping = true;
  list_for_each(q, &p->threads) {
    struct task *t = list_entry(q, struct task, thread_link);
    if (t->state == T_RUNNING)
      return 'R';
    if (t->state == T_UNINTERRUPTIBLE)
      sleeping = false;
  }
  return sleeping ? 'S' : 'D';
}

static uint64_t tty_nr(struct proc *p) {
  struct tty *t = p->ctty;
  if (!t)
    return 0;
  if (t->pty)
    return MKDEV(136, t->index);
  return MKDEV(4, 64);
}

static uint64_t vsize(struct mm *mm) {
  uint64_t v = 0;
  if (mm)
    list_for_each(q, &mm->vmas) {
      struct vma *a = list_entry(q, struct vma, link);
      v += a->end - a->start;
    }
  return v;
}

static void mounts_text(struct sbuf *s, bool info) {
  int id = 20;
  list_for_each(q, &mounts) {
    struct mount *m = list_entry(q, struct mount, link);
    char path[1024];
    if (path_string(&m->mountpoint, path, sizeof(path)))
      strcpy(path, "?");
    if (m == root_mount)
      strcpy(path, "/");
    const char *opt = (m->flags & MS_RDONLY) || m->sb->rdonly ? "ro" : "rw";
    if (info) {
      int parent = 20;
      int k = 20;
      list_for_each(r, &mounts) {
        if (list_entry(r, struct mount, link) == m->parent)
          parent = k;
        k++;
      }
      s->len += 0;
      sb_printf(s, "%d %d %u:%u / %s %s - %s %s %s\n", id, parent, MAJOR(m->sb->dev), MINOR(m->sb->dev), path,
                opt, m->fstype, m->devname, opt);
    } else
      sb_printf(s, "%s %s %s %s 0 0\n", m->devname, path, m->fstype, opt);
    id++;
  }
}

static void ipc_text(struct sbuf *s, int which);

static int make_text(struct inode *i, struct sbuf *s) {
  ino_t ino = i->ino;
  uint64_t up = monotonic_ns();
  switch (ino) {
  case R_MEMINFO:
    sb_printf(s, "MemTotal:       %8lu kB\nMemFree:        %8lu kB\nMemAvailable:   %8lu kB\n"
                 "Buffers:        %8lu kB\nCached:                0 kB\nSwapTotal:             0 kB\n"
                 "SwapFree:              0 kB\n",
              total_pages * 4, free_pages_count * 4, free_pages_count * 4 + bcache_kb(),
              bcache_kb());
    return 0;
  case R_UPTIME:
    sb_printf(s, "%lu.%02lu 0.00\n", up / 1000000000UL, up % 1000000000UL / 10000000UL);
    return 0;
  case R_VERSION:
    sb_printf(s, "%s version %s %s\n", uts_sysname, uts_release, uts_version);
    return 0;
  case R_STAT: {
    uint64_t ut = 0, st = 0;
    int procs = 0, running = 0;
    list_for_each(q, &all_procs) {
      struct proc *p = list_entry(q, struct proc, all_link);
      ut += p->utime;
      st += p->stime;
      procs++;
      if (state_char(p) == 'R')
        running++;
    }
    uint64_t total = jiffies * 100 / HZ;
    uint64_t u = ut * 100 / HZ, sy = st * 100 / HZ;
    uint64_t idle = total > u + sy ? total - u - sy : 0;
    sb_printf(s, "cpu  %lu 0 %lu %lu 0 0 0 0 0 0\ncpu0 %lu 0 %lu %lu 0 0 0 0 0 0\n", u, sy, idle, u, sy, idle);
    sb_printf(s, "btime %lu\nprocesses %d\nprocs_running %d\nprocs_blocked 0\n",
              (realtime_ns() - up) / 1000000000UL, procs, running);
    return 0;
  }
  case R_CPUINFO: {
    uint32_t r[4];
    char vendor[13];
    cpuid4(0, 0, r);
    memcpy(vendor, &r[1], 4);
    memcpy(vendor + 4, &r[3], 4);
    memcpy(vendor + 8, &r[2], 4);
    vendor[12] = 0;
    sb_printf(s, "processor\t: 0\nvendor_id\t: %s\ncpu cores\t: 1\n\n", vendor);
    return 0;
  }
  case R_FILESYSTEMS:
    sb_printf(s, "\text2\nnodev\ttmpfs\nnodev\tdevtmpfs\nnodev\tproc\nnodev\tramfs\n");
    return 0;
  case R_LOADAVG:
    sb_printf(s, "0.00 0.00 0.00 1/1 %d\n", cur_proc()->pid);
    return 0;
  case R_NETDEV:
    sb_printf(s, "Inter-|   Receive                            "
                 "                    |  Transmit\n"
                 " face |bytes    packets errs drop fifo frame compressed multicast"
                 "|bytes    packets errs drop fifo colls carrier compressed\n");
    sb_printf(s, "    lo:       0       0    0    0    0     0          0         0"
                 "        0       0    0    0    0     0       0          0\n");
    if (eth0.present)
      sb_printf(s, "  eth0: %7lu %7lu    0 %4lu    0     0          0         0 %8lu %7lu    0 %4lu    0     0       0          0\n",
                eth0.rx_bytes, eth0.rx_packets, eth0.rx_dropped, eth0.tx_bytes, eth0.tx_packets, eth0.tx_dropped);
    return 0;
  case R_NETPNP:
    net_pnp((void (*)(void *, const char *, ...))sb_printf, s);
    return 0;
  case R_MSGMNB:
    sb_printf(s, "16384\n");
    return 0;
  case R_PIDMAX:
    sb_printf(s, "32768\n");
    return 0;
  case R_HOSTNAME:
    sb_printf(s, "%s\n", uts_nodename);
    return 0;
  case R_OSTYPE:
    sb_printf(s, "%s\n", uts_sysname);
    return 0;
  case R_OSRELEASE:
    sb_printf(s, "%s\n", uts_release);
    return 0;
  case R_NGROUPS:
    sb_printf(s, "65536\n");
    return 0;
  case R_IPCMSG:
  case R_IPCSHM:
  case R_IPCSEM:
    ipc_text(s, (int)(ino - R_IPCMSG));
    return 0;
  case R_CMDLINE:
    sb_printf(s, "%s\n", kernel_cmdline);
    return 0;
  }
  struct task *th = NULL;
  struct proc *p;
  if (ino & TBIT) {
    th = find_task((pid_t)((ino & ~TBIT) >> 12));
    p = th ? th->proc : NULL;
  } else
    p = proc_of((pid_t)(ino >> 12));
  if (!p)
    return -ESRCH;
  int sub = (int)(ino & 0xfff);
  struct mm *mm = p->mm;
  switch (sub) {
  case P_STAT: {
    int nthreads = p->nthreads ? p->nthreads : 1;
    struct task *t0 = th ? th : list_empty(&p->threads) ? NULL : list_entry(p->threads.next, struct task, thread_link);
    sigset_t ign = 0, caught = 0;
    for (int k = 0; k < NSIG; k++) {
      if (p->sigact[k].handler == SIG_IGN)
        ign |= 1UL << k;
      else if (p->sigact[k].handler != SIG_DFL)
        caught |= 1UL << k;
    }
    struct tty *tt = p->ctty;
    sb_printf(s, "%d (%s) %c %d %d %d %lu %d %u ", th ? th->tid : p->pid, p->comm, state_char(p),
              p == init_proc ? 0 : p->parent->pid, p->pgid, p->sid, tty_nr(p), tt ? tt->pgrp : -1, 0x400000U);
    sb_printf(s, "0 0 0 0 %lu %lu %lu %lu %d %d %d 0 %lu %lu %lu %lu ", p->utime * 100 / HZ, p->stime * 100 / HZ,
              p->cutime * 100 / HZ, p->cstime * 100 / HZ, t0 && task_prio(t0) ? -1 - task_prio(t0) : 20 + p->nice,
              p->nice, nthreads, p->start_time / 10000000UL, vsize(mm), mm ? mm->rss : 0, RLIM_INFINITY);
    sb_printf(s, "0 0 %lu 0 0 %lu %lu %lu %lu 0 0 0 %d 0 %d %d 0 0 0 0 0 0 0 0 0 0 %d\n",
              mm ? mm->stack_top : 0, p->pending.set, t0 ? t0->blocked : 0, ign, caught,
              SIGCHLD, t0 ? t0->rtprio : 0, t0 ? t0->policy : 0, p->state == P_ZOMBIE ? p->exit_status : 0);
    return 0;
  }
  case P_STATUS: {
    const char *st = "S (sleeping)";
    switch (state_char(p)) {
    case 'R': st = "R (running)"; break;
    case 'T': st = "T (stopped)"; break;
    case 'Z': st = "Z (zombie)"; break;
    case 'D': st = "D (disk sleep)"; break;
    }
    struct cred *c = &p->cred;
    sb_printf(s, "Name:\t%s\nUmask:\t%04o\nState:\t%s\nTgid:\t%d\nPid:\t%d\nPPid:\t%d\n", p->comm, p->umask, st,
              p->pid, p->pid, p == init_proc ? 0 : p->parent->pid);
    sb_printf(s, "Uid:\t%u\t%u\t%u\t%u\nGid:\t%u\t%u\t%u\t%u\nGroups:\t", c->uid, c->euid, c->suid, c->fsuid,
              c->gid, c->egid, c->sgid, c->fsgid);
    for (int k = 0; k < c->ngroups; k++)
      sb_printf(s, "%u ", c->groups[k]);
    sb_printf(s, "\nVmSize:\t%8lu kB\nVmRSS:\t%8lu kB\nThreads:\t%d\nSigPnd:\t%016lx\n", vsize(mm) / 1024,
              mm ? mm->rss * 4 : 0, p->nthreads, p->pending.set);
    return 0;
  }
  case P_CMDLINE:
  case P_ENVIRON: {
    if (!mm)
      return 0;
    uint64_t a = sub == P_CMDLINE ? mm->arg_start : mm->env_start;
    uint64_t e = sub == P_CMDLINE ? mm->arg_end : mm->env_end;
    if (e <= a || e - a > (1UL << 20))
      return 0;
    char *tmp = kmalloc(e - a);
    if (!tmp)
      return -ENOMEM;
    /* read the other address space */
    size_t done = 0;
    while (done < e - a) {
      uint64_t va = a + done;
      uint64_t *pte = pte_get(mm->pml4, va, false);
      if (!pte || !(*pte & PTE_P)) {
        if (handle_fault(mm, va, false, false))
          break;
        pte = pte_get(mm->pml4, va, false);
      }
      size_t c = MIN(e - a - done, PAGE_SIZE - (va & 0xfff));
      memcpy(tmp + done, (char *)P2V(*pte & PTE_ADDR) + (va & 0xfff), c);
      done += c;
    }
    sb_bytes(s, tmp, done);
    kfree(tmp);
    return 0;
  }
  case P_COMM:
    sb_printf(s, "%s\n", p->comm);
    return 0;
  case P_WCHAN:
    sb_printf(s, "%s", state_char(p) == 'S' ? "do_wait" : "0");
    return 0;
  case P_MAPS:
    if (mm)
      list_for_each(q, &mm->vmas) {
        struct vma *v = list_entry(q, struct vma, link);
        unsigned ma = 0, mi = 0;
        uint64_t ino2 = 0;
        const char *name = "";
        if (v->file && v->file->ino) {
          ma = MAJOR(v->file->ino->sb->dev);
          mi = MINOR(v->file->ino->sb->dev);
          ino2 = v->file->ino->ino;
          name = v->file->path ? v->file->path : "";
        } else if (v->flags & VM_GROWSDOWN)
          name = "[stack]";
        else if (v->start >= mm->brk_start && v->end <= PAGE_UP(mm->brk) && mm->brk_start)
          name = "[heap]";
        sb_printf(s, "%012lx-%012lx %c%c%c%c %08lx %02x:%02x %lu %s%s\n", v->start, v->end,
                  v->flags & VM_READ ? 'r' : '-', v->flags & VM_WRITE ? 'w' : '-', v->flags & VM_EXEC ? 'x' : '-',
                  v->flags & VM_SHARED ? 's' : 'p', v->offset, ma, mi, ino2, *name ? "    " : "", name);
      }
    return 0;
  case P_MOUNTS:
    mounts_text(s, false);
    return 0;
  case P_MOUNTINFO:
    mounts_text(s, true);
    return 0;
  }
  return -EINVAL;
}

struct ptext {
  char *b;
  size_t len;
};

static int proc_open(struct inode *i, struct file *f) {
  struct sbuf s = {0};
  int e = make_text(i, &s);
  if (e) {
    kfree(s.b);
    return e;
  }
  struct ptext *t = kmalloc(sizeof(*t));
  t->b = s.b;
  t->len = s.len;
  f->priv = t;
  return 0;
}

static void proc_release(struct file *f) {
  struct ptext *t = f->priv;
  if (t) {
    kfree(t->b);
    kfree(t);
  }
}

static ssize_t proc_read(struct file *f, char *buf, size_t n, off_t *pos) {
  struct ptext *t = f->priv;
  if (!t || (uint64_t)*pos >= t->len)
    return 0;
  size_t c = MIN(n, t->len - (size_t)*pos);
  if (copy_to_user(buf, t->b + *pos, c))
    return -EFAULT;
  *pos += (off_t)c;
  return (ssize_t)c;
}

static ssize_t proc_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)pos;
  ino_t ino = f->ino->ino;
  if (ino >= R_LAST && (ino & 0xfff) == P_COMM) {
    struct proc *p = proc_of((pid_t)(ino >> 12));
    if (!p)
      return -ESRCH;
    char c[16];
    size_t k = MIN(n, 15UL);
    if (copy_from_user(c, buf, k))
      return -EFAULT;
    while (k && c[k - 1] == '\n')
      k--;
    c[k] = 0;
    memset(p->comm, 0, 16);
    memcpy(p->comm, c, k);
    return (ssize_t)n;
  }
  return -EACCES;
}

static void proc_put(struct inode *i) { (void)i; }

static const struct inode_ops proc_dir_iops = {.lookup = proc_lookup};
static int proc_follow(struct inode *i, struct path *out) {
  ino_t ino = i->ino;
  struct proc *p = proc_of((pid_t)(ino >> 12));
  int sub = (int)(ino & 0xfff);
  if (!p)
    return -ENOENT;
  if (sub == P_CWD || sub == P_ROOT) {
    *out = sub == P_CWD ? p->cwd : p->root;
    if (!out->ino)
      return -ENOENT;
    path_get(out);
    return 0;
  }
  if (sub == P_EXE)
    return p->exe ? namei_at(AT_FDCWD, p->exe, LOOKUP_FOLLOW, out) : -ENOENT;
  int fd = sub & 0x7ff;
  if (!(sub & P_FD) || fd >= p->nfd || !p->fd[fd] || !p->fd[fd]->ino)
    return -ENOENT;
  struct file *f = p->fd[fd];
  out->mnt = f->mnt ? f->mnt : root_mount;
  out->ino = igrab(f->ino);
  return 0;
}

/* /proc/self, thread-self, mounts are ordinary links; the rest are magic */
static const struct inode_ops proc_link_iops = {.readlink = proc_readlink};
static const struct inode_ops proc_magic_iops = {.readlink = proc_readlink, .follow = proc_follow};
static const struct inode_ops proc_file_iops = {.truncate = NULL};
static const struct file_ops proc_dir_fops = {.readdir = proc_readdir, .seekable = true};
static const struct file_ops proc_file_fops = {
    .open = proc_open, .release = proc_release, .read = proc_read, .write = proc_write, .seekable = true};
static const struct super_ops proc_sops = {.read_inode = proc_read_inode, .put = proc_put};

static struct fs_type proc_type;

static struct super *proc_mount(struct blockdev *b, int flags, const char *data) {
  (void)b, (void)flags, (void)data;
  struct super *sb = super_alloc(&proc_type, NULL);
  sb->ops = &proc_sops;
  sb->blocksize = 1024;
  sb->magic = PROC_MAGIC;
  struct inode *root = iget(sb, R_ROOT);
  if (IS_ERR(root))
    return (struct super *)root;
  sb->root = root;
  return sb;
}

static struct fs_type proc_type = {.name = "proc", .mount = proc_mount};

void procfs_register(void) { register_fs(&proc_type); }

/* SysV IPC tables: kern/ipc.c fills these */
void ipc_proc_text(int which, void (*emit)(void *ctx, const char *line), void *ctx);

static void emit_line(void *ctx, const char *line) { sb_printf(ctx, "%s", line); }

static void ipc_text(struct sbuf *s, int which) {
  static const char *hdr[3] = {
      "       key      msqid perms      cbytes       qnum lspid lrpid   uid   gid  cuid  cgid      stime      rtime      ctime\n",
      "       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime                   rss                  swap\n",
      "       key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n"};
  sb_printf(s, "%s", hdr[which]);
  ipc_proc_text(which, emit_line, s);
}

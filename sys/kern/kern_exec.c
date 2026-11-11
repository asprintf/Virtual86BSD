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

/* exec.c - execve: ELF64 programs (ET_EXEC, ET_DYN, with or without an
   interpreter), "#!" scripts, the initial stack of Linux. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>

extern const struct sysent sysent[];

/* V86BSD's own ABI: the numbers of syscalls.master */
const struct emul emul_native = {
    "native", sysent, SYS_MAXSYSCALL, SYS_rt_sigreturn, ostype, osrelease,
};

struct elf_ehdr {
  uint8_t ident[16];
  uint16_t type, machine;
  uint32_t version;
  uint64_t entry, phoff, shoff;
  uint32_t flags;
  uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct elf_phdr {
  uint32_t type, flags;
  uint64_t offset, vaddr, paddr, filesz, memsz, align;
};

#define PT_LOAD 1
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_PHDR 6
#define PT_GNU_STACK 0x6474e551
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define ET_EXEC 2
#define ET_DYN 3

#define ARG_MAX (2UL << 20)
int argmax = ARG_MAX; /* kern.argmax */
#define STACK_TOP 0x7ffffffff000UL
#define DYN_BASE 0x555555554000UL

/* arguments and environment, copied into the kernel */
struct args {
  char *buf;
  size_t len, cap;
  int argc, envc;
};

static int args_add(struct args *a, const char *s, size_t n) {
  if (a->len + n + 1 > ARG_MAX)
    return -E2BIG;
  if (a->len + n + 1 > a->cap) {
    size_t nc = a->cap ? a->cap : 16384;
    while (nc < a->len + n + 1)
      nc *= 2;
    char *nb = krealloc(a->buf, a->cap, nc);
    if (!nb)
      return -ENOMEM;
    a->buf = nb;
    a->cap = nc;
  }
  memcpy(a->buf + a->len, s, n);
  a->buf[a->len + n] = 0;
  a->len += n + 1;
  return 0;
}

static int args_copy(struct args *a, char *const *uv, int *count) {
  *count = 0;
  if (!uv)
    return 0;
  for (int i = 0;; i++) {
    char *up;
    if (copy_from_user(&up, uv + i, sizeof(up)))
      return -EFAULT;
    if (!up)
      break;
    long n;
    if ((uint64_t)up >= USER_TOP)
      n = (long)strlen(up);
    else {
      n = user_strnlen(up, MIN(USER_TOP - (uint64_t)up, 131072));
      if (n < 0)
        return -EFAULT;
      if (n >= 131072)
        return -E2BIG;
    }
    if (a->len + (size_t)n + 1 > ARG_MAX)
      return -E2BIG;
    size_t at = a->len;
    int e = args_add(a, "", (size_t)n); /* make room */
    if (e)
      return e;
    if (copy_from_user(a->buf + at, up, (size_t)n))
      return -EFAULT;
    a->buf[at + n] = 0;
    (*count)++;
  }
  return 0;
}

/* write into another address space (the new one) */
static int mm_write(struct mm *mm, uint64_t va, const void *src, size_t n) {
  const char *s = src;
  while (n) {
    if (handle_fault(mm, va, true, false))
      return -EFAULT;
    uint64_t *pte = pte_get(mm->pml4, va, false);
    if (!pte || !(*pte & PTE_P))
      return -EFAULT;
    size_t chunk = MIN(n, PAGE_SIZE - (va & 0xfff));
    memcpy((char *)P2V(*pte & PTE_ADDR) + (va & 0xfff), s, chunk);
    s += chunk;
    va += chunk;
    n -= chunk;
  }
  return 0;
}

static int mm_zero(struct mm *mm, uint64_t va, size_t n) {
  static const char zero[256];
  while (n) {
    size_t c = MIN(n, sizeof(zero));
    int e = mm_write(mm, va, zero, c);
    if (e)
      return e;
    va += c;
    n -= c;
  }
  return 0;
}

static unsigned pflags(uint32_t f) {
  return (f & PF_R ? VM_READ : 0) | (f & PF_W ? VM_WRITE : 0) | (f & PF_X ? VM_EXEC : 0);
}

struct loaded {
  uint64_t bias, entry, phdr, phnum, end;
};

static int read_ehdr(struct file *f, struct elf_ehdr *eh, struct elf_phdr **ph) {
  if (kernel_read(f, eh, sizeof(*eh), 0) != sizeof(*eh))
    return -ENOEXEC;
  if (memcmp(eh->ident, "\177ELF", 4) || eh->ident[4] != 2 || eh->ident[5] != 1)
    return -ENOEXEC;
  if (eh->machine != 62 || (eh->type != ET_EXEC && eh->type != ET_DYN))
    return -ENOEXEC;
  if (eh->phentsize != sizeof(struct elf_phdr) || eh->phnum == 0 || eh->phnum > 128)
    return -ENOEXEC;
  size_t sz = eh->phnum * sizeof(struct elf_phdr);
  *ph = kmalloc(sz);
  if (!*ph)
    return -ENOMEM;
  if (kernel_read(f, *ph, sz, (off_t)eh->phoff) != (ssize_t)sz) {
    kfree(*ph);
    return -ENOEXEC;
  }
  return 0;
}

static int load_elf(struct mm *mm, struct file *f, struct elf_ehdr *eh, struct elf_phdr *ph,
                    bool interp, struct loaded *out) {
  uint64_t lo = ~0UL, hi = 0;
  for (int i = 0; i < eh->phnum; i++)
    if (ph[i].type == PT_LOAD) {
      if (ph[i].vaddr < lo)
        lo = ph[i].vaddr;
      if (ph[i].vaddr + ph[i].memsz > hi)
        hi = ph[i].vaddr + ph[i].memsz;
    }
  if (lo == ~0UL)
    return -ENOEXEC;
  lo = PAGE_DOWN(lo);
  hi = PAGE_UP(hi);
  uint64_t bias = 0;
  if (eh->type == ET_DYN) {
    if (interp) {
      uint64_t at = vma_free_area(mm, hi - lo, 0);
      if (!at)
        return -ENOMEM;
      bias = at - lo;
    } else
      bias = DYN_BASE - lo;
  }
  if (hi + bias > USER_TOP)
    return -ENOEXEC;
  uint64_t phdr = 0;
  for (int i = 0; i < eh->phnum; i++) {
    struct elf_phdr *p = &ph[i];
    if (p->type == PT_PHDR)
      phdr = p->vaddr + bias;
    if (p->type != PT_LOAD || p->memsz == 0)
      continue;
    if ((p->vaddr & 0xfff) != (p->offset & 0xfff) || p->filesz > p->memsz)
      return -ENOEXEC;
    uint64_t start = PAGE_DOWN(p->vaddr) + bias;
    uint64_t fend = p->vaddr + p->filesz + bias;
    uint64_t mend = PAGE_UP(p->vaddr + p->memsz + bias);
    unsigned fl = pflags(p->flags);
    if (p->filesz) {
      struct vma *v = vma_new(mm, start, PAGE_UP(fend), fl);
      if (!v)
        return -ENOMEM;
      v->file = f;
      file_get(f);
      v->offset = PAGE_DOWN(p->offset);
      /* the rest of the last file page belongs to the bss */
      if ((fend & 0xfff) && p->memsz > p->filesz) {
        unsigned saved = v->flags;
        v->flags |= VM_WRITE;
        int e = mm_zero(mm, fend, PAGE_UP(fend) - fend);
        v->flags = saved;
        if (e)
          return e;
        /* the page is a private copy now: keep it writable only if allowed */
        uint64_t *pte = pte_get(mm->pml4, PAGE_DOWN(fend), false);
        if (pte && !(saved & VM_WRITE))
          *pte &= ~PTE_W;
      }
    }
    uint64_t bss = PAGE_UP(fend);
    if (!p->filesz)
      bss = start;
    if (mend > bss && !vma_new(mm, bss, mend, fl))
      return -ENOMEM;
    if (!phdr && eh->phoff >= p->offset && eh->phoff < p->offset + p->filesz)
      phdr = p->vaddr + (eh->phoff - p->offset) + bias;
  }
  out->bias = bias;
  out->entry = eh->entry + bias;
  out->phdr = phdr;
  out->phnum = eh->phnum;
  out->end = hi + bias;
  return 0;
}

/* "#!interpreter [arg]": rewrite the arguments */
static int script_args(struct args *a, const char *head, size_t n, const char *path,
                       char **interp) {
  size_t end = 2;
  while (end < n && head[end] != '\n')
    end++;
  if (end == n && n == 256)
    return -ENOEXEC; /* line too long (Linux: ENOEXEC) */
  char line[256];
  memcpy(line, head + 2, end - 2);
  line[end - 2] = 0;
  char *s = line;
  while (*s == ' ' || *s == '\t')
    s++;
  if (!*s)
    return -ENOEXEC;
  char *e = s;
  while (*e && *e != ' ' && *e != '\t')
    e++;
  char *opt = NULL;
  if (*e) {
    *e++ = 0;
    while (*e == ' ' || *e == '\t')
      e++;
    if (*e) {
      opt = e;
      size_t ol = strlen(opt);
      while (ol && (opt[ol - 1] == ' ' || opt[ol - 1] == '\t'))
        opt[--ol] = 0;
    }
  }
  /* new: interp [opt] path argv[1..] env */
  struct args na = {0};
  int r = args_add(&na, s, strlen(s));
  if (!r && opt)
    r = args_add(&na, opt, strlen(opt));
  if (!r)
    r = args_add(&na, path, strlen(path));
  /* keep argv[1..] and the environment */
  size_t skip = a->argc ? strlen(a->buf) + 1 : 0;
  for (char *q = a->buf + skip; !r && q < a->buf + a->len; q += strlen(q) + 1)
    r = args_add(&na, q, strlen(q));
  if (r) {
    kfree(na.buf);
    return r;
  }
  na.argc = (a->argc ? a->argc - 1 : 0) + 2 + (opt ? 1 : 0);
  na.envc = a->envc;
  kfree(a->buf);
  *a = na;
  *interp = kstrdup(s);
  return 0;
}

static void de_thread(void) {
  struct proc *p = cur_proc();
  if (p->nthreads == 1)
    return;
  uint64_t f = irq_save();
  list_for_each(q, &p->threads) {
    struct task *t = list_entry(q, struct task, thread_link);
    if (t != current) {
      t->pending.set |= SIGBIT(SIGKILL);
      wake_task(t);
      if (t->state == T_STOPPED) {
        t->state = T_RUNNING;
        make_runnable(t);
      }
    }
  }
  irq_restore(f);
  while (p->nthreads > 1)
    sleep_ns(1000000);
  current->tid = p->pid; /* the survivor takes over the process id */
}

static uint64_t hwcap(void) {
  uint32_t r[4];
  cpuid4(1, 0, r);
  return r[3];
}

static int build_stack(struct mm *mm, struct args *a, const char *execfn, struct loaded *prog,
                       uint64_t interp_base, bool secure, uint64_t *sp_out) {
  struct proc *p = cur_proc();
  uint64_t top = STACK_TOP;
  uint64_t stack_lim = MIN(p->rlim[RLIMIT_STACK].cur, 1UL << 30);
  if (stack_lim < 128UL << 10)
    stack_lim = 128UL << 10;
  struct vma *v = vma_new(mm, top - 128 * 1024, top, VM_READ | VM_WRITE | VM_GROWSDOWN);
  if (!v)
    return -ENOMEM;
  mm->stack_top = top;
  uint64_t sp = top - 8;
  /* execfn, then the strings */
  size_t fl = strlen(execfn) + 1;
  sp -= fl;
  uint64_t execfn_at = sp;
  if (mm_write(mm, sp, execfn, fl))
    return -EFAULT;
  sp -= a->len;
  uint64_t strings = sp;
  if (a->len > 128 * 1024 - 4096) {
    /* grow the stack VMA for big argument lists */
    v->start = PAGE_DOWN(sp - 65536);
  }
  if (mm_write(mm, sp, a->buf, a->len))
    return -EFAULT;
  mm->arg_start = strings;
  sp -= 7;
  uint64_t platform = sp;
  if (mm_write(mm, sp, "x86_64", 7))
    return -EFAULT;
  sp -= 16;
  uint64_t random = sp;
  uint8_t rnd[16];
  get_random_bytes(rnd, 16);
  if (mm_write(mm, sp, rnd, 16))
    return -EFAULT;
  sp &= ~15UL;
  uint64_t auxv[2 * 22];
  int na = 0;
#define AUX(k, v) (auxv[na++] = (k), auxv[na++] = (v))
  AUX(16, hwcap());
  AUX(6, PAGE_SIZE);
  AUX(17, 100);
  AUX(3, prog->phdr);
  AUX(4, sizeof(struct elf_phdr));
  AUX(5, prog->phnum);
  AUX(7, interp_base);
  AUX(8, 0);
  AUX(9, prog->entry);
  AUX(11, p->cred.uid);
  AUX(12, p->cred.euid);
  AUX(13, p->cred.gid);
  AUX(14, p->cred.egid);
  AUX(23, secure ? 1 : 0);
  AUX(25, random);
  AUX(26, 0);
  AUX(31, execfn_at);
  AUX(15, platform);
  AUX(51, 2048);
  AUX(0, 0);
#undef AUX
  int nptr = 1 + a->argc + 1 + a->envc + 1;
  size_t total = (size_t)nptr * 8 + (size_t)na * 8;
  sp -= total;
  sp &= ~15UL;
  uint64_t *vec = kmalloc(total);
  if (!vec)
    return -ENOMEM;
  int k = 0;
  vec[k++] = (uint64_t)a->argc;
  uint64_t s = strings;
  for (int i = 0; i < a->argc; i++) {
    vec[k++] = s;
    s += strlen(a->buf + (s - strings)) + 1;
  }
  vec[k++] = 0;
  mm->arg_end = s;
  mm->env_start = s;
  for (int i = 0; i < a->envc; i++) {
    vec[k++] = s;
    s += strlen(a->buf + (s - strings)) + 1;
  }
  mm->env_end = s;
  vec[k++] = 0;
  memcpy(vec + k, auxv, (size_t)na * 8);
  int e = mm_write(mm, sp, vec, total);
  kfree(vec);
  if (e)
    return e;
  *sp_out = sp;
  return 0;
}

/* The brand of a program: V86BSD's own, a PT_NOTE holding the note named
   "V86BSD" of type 1 (libc's start files, lib/libc/config/note_native.s).
   The file is the user's: every length is checked. */
#define NT_V86BSD_IDENT 1
#define NOTE_MAX 256

static bool elf_branded(struct file *f, const struct elf_phdr *ph, int n) {
  uint32_t buf[NOTE_MAX / 4];
  for (int i = 0; i < n; i++) {
    if (ph[i].type != PT_NOTE || ph[i].filesz < 12 || ph[i].filesz > NOTE_MAX)
      continue;
    size_t len = ph[i].filesz;
    if (kernel_read(f, buf, len, (off_t)ph[i].offset) != (ssize_t)len)
      continue;
    for (size_t off = 0; off + 12 <= len;) {
      uint32_t namesz = buf[off / 4], descsz = buf[off / 4 + 1], type = buf[off / 4 + 2];
      if (namesz > len || descsz > len)
        break;
      size_t nsz = (namesz + 3) & ~3UL, dsz = (descsz + 3) & ~3UL;
      if (off + 12 + nsz + dsz > len)
        break;
      if (type == NT_V86BSD_IDENT && namesz == 7 && !memcmp((char *)buf + off + 12, "V86BSD", 7))
        return true;
      off += 12 + nsz + dsz;
    }
  }
  return false;
}

static int exec_file(struct file *f, struct args *a, const char *execfn, const char *abspath) {
  struct proc *p = cur_proc();
  struct elf_ehdr eh;
  struct elf_phdr *ph = NULL;
  int e = read_ehdr(f, &eh, &ph);
  if (e)
    return e;
  /* the ABI: V86BSD's if the program is branded, else Linux's (when the
     kernel has the emulation) */
  const struct emul *emul = &emul_native;
  if (!elf_branded(f, ph, eh.phnum)) {
#ifdef COMPAT_LINUX
    emul = &emul_linux;
#else
    kfree(ph);
    return -ENOEXEC;
#endif
  }
  char *interp = NULL;
  struct file *ifile = NULL;
  for (int i = 0; i < eh.phnum; i++)
    if (ph[i].type == PT_INTERP) {
      if (ph[i].filesz < 2 || ph[i].filesz > 4096) {
        e = -ENOEXEC;
        goto out;
      }
      interp = kmalloc(ph[i].filesz + 1);
      if (!interp || kernel_read(f, interp, ph[i].filesz, (off_t)ph[i].offset) != (ssize_t)ph[i].filesz) {
        e = -ENOEXEC;
        goto out;
      }
      interp[ph[i].filesz] = 0;
      ifile = open_path(AT_FDCWD, interp, O_RDONLY, 0, &e);
      if (!ifile)
        goto out;
      break;
    }
  struct mm *mm = mm_new();
  if (!mm) {
    e = -ENOMEM;
    goto out;
  }
  struct loaded prog, ld = {0};
  e = load_elf(mm, f, &eh, ph, false, &prog);
  if (!e && ifile) {
    struct elf_ehdr ieh;
    struct elf_phdr *iph;
    e = read_ehdr(ifile, &ieh, &iph);
    if (!e) {
      e = load_elf(mm, ifile, &ieh, iph, true, &ld);
      kfree(iph);
    }
  }
  mm->brk_start = mm->brk = PAGE_UP(prog.end);
  /* set-user-ID / set-group-ID */
  struct inode *ino = f->ino;
  bool nosuid = f->mnt && (f->mnt->flags & MS_NOSUID);
  uid_t neuid = p->cred.euid;
  gid_t negid = p->cred.egid;
  if (!nosuid && (ino->mode & S_ISUID))
    neuid = ino->uid;
  if (!nosuid && (ino->mode & (S_ISGID | 00010)) == (S_ISGID | 00010))
    negid = ino->gid;
  bool secure = neuid != p->cred.uid || negid != p->cred.gid;
  uint64_t sp = 0;
  if (!e) {
    uid_t ou = p->cred.euid;
    gid_t og = p->cred.egid;
    p->cred.euid = neuid;
    p->cred.egid = negid;
    e = build_stack(mm, a, execfn, &prog, ld.bias, secure, &sp);
    p->cred.euid = ou;
    p->cred.egid = og;
  }
  if (e) {
    mm_put(mm);
    goto out;
  }

  /* the point of no return */
  de_thread();
  struct task *t = current;
  if (t->clear_child_tid) {
    int32_t z = 0;
    if (copy_to_user((void *)t->clear_child_tid, &z, 4) == 0)
      futex_wake_addr(t->clear_child_tid, 1);
    t->clear_child_tid = 0;
  }
  t->robust_list = 0;
  struct mm *old = p->mm;
  p->mm = mm;
  mm_switch(mm);
  mm_put(old);
  p->cred.euid = p->cred.suid = p->cred.fsuid = neuid;
  p->cred.egid = p->cred.sgid = p->cred.fsgid = negid;
  fd_close_on_exec(p);
  signals_exec_reset(p);
  timers_exit_posix(p);
  current->altstack.flags = SS_DISABLE;
  const char *base = strrchr(abspath, '/');
  base = base ? base + 1 : abspath;
  memset(p->comm, 0, sizeof(p->comm));
  memcpy(p->comm, base, MIN(strlen(base), 15));
  kfree(p->exe);
  p->exe = kstrdup(abspath);
  p->did_exec = true;
  p->p_emul = emul;
  proc_vfork_release(p);
  t->fsbase = 0;
  wrmsr(MSR_FS_BASE, 0);
  fninit_sse();
  fxsave(t->fpu);
  struct regs *r = t->regs;
  memset(r, 0, sizeof(*r));
  r->rip = ifile ? ld.entry : prog.entry;
  r->rsp = sp;
  r->cs = USER_CS;
  r->ss = USER_DS;
  r->rflags = 0x202;
  e = 0;
out:
  kfree(ph);
  kfree(interp);
  if (ifile)
    file_put(ifile);
  return e;
}

int do_execve(const char *upath, char *const *uargv, char *const *uenvp, int dirfd, int flags) {
  char *path = user_path(upath);
  if (IS_ERR(path))
    return (int)PTR_ERR(path);
  struct args a = {0};
  int e = args_copy(&a, uargv, &a.argc);
  if (!e) {
    int envc;
    e = args_copy(&a, uenvp, &envc);
    a.envc = envc;
  }
  char *cur = kstrdup(path);
  struct file *f = NULL;
  for (int depth = 0; !e; depth++) {
    if (depth > 4) {
      e = -ELOOP;
      break;
    }
    int of = O_RDONLY | ((flags & AT_SYMLINK_NOFOLLOW) ? O_NOFOLLOW : 0);
    if ((flags & AT_EMPTY_PATH) && !*cur && depth == 0) {
      struct file *df = fd_get(dirfd);
      if (!df) {
        e = -EBADF;
        break;
      }
      f = open_inode(df->ino, df->mnt, O_RDONLY, &e);
    } else
      f = open_path(depth ? AT_FDCWD : dirfd, cur, of, 0, &e);
    if (!f)
      break;
    struct inode *ino = f->ino;
    if (!S_ISREG(ino->mode)) {
      e = -EACCES;
      break;
    }
    if (permission(ino, MAY_EXEC) || (f->mnt && (f->mnt->flags & MS_NOEXEC))) {
      e = -EACCES;
      break;
    }
    char head[256];
    ssize_t n = kernel_read(f, head, sizeof(head), 0);
    if (n < 0) {
      e = (int)n;
      break;
    }
    if (n >= 2 && head[0] == '#' && head[1] == '!') {
      char *interp;
      e = script_args(&a, head, (size_t)n, cur, &interp);
      if (e)
        break;
      file_put(f);
      f = NULL;
      kfree(cur);
      cur = interp;
      continue;
    }
    char *abspath = f->path ? f->path : cur;
    e = exec_file(f, &a, path, abspath);
    break;
  }
  if (f)
    file_put(f);
  kfree(cur);
  kfree(a.buf);
  kfree(path);
  return e;
}

long sys_execve(long path, long argv, long envp) {
  return do_execve((const char *)path, (char *const *)argv, (char *const *)envp, AT_FDCWD, 0);
}

long sys_execveat(long dirfd, long path, long argv, long envp, long flags) {
  if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
    return -EINVAL;
  return do_execve((const char *)path, (char *const *)argv, (char *const *)envp, (int)dirfd, (int)flags);
}

int kernel_execve(const char *path, const char *const *argv, const char *const *envp) {
  return do_execve(path, (char *const *)argv, (char *const *)envp, AT_FDCWD, 0);
}

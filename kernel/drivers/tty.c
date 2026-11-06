/* tty.c - the terminal line discipline (as Linux's N_TTY, in part),
   the console terminal, /dev/tty, and pseudo-terminals. */
#include "kernel.h"
#include "arch.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include "dev.h"
#include "tty.h"
#include "syscall.h"

/* termios bits (Linux) */
#define IGNBRK 0000001
#define BRKINT 0000002
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IUCLC 0001000
#define IXON 0002000
#define IXANY 0004000
#define IXOFF 0010000
#define IMAXBEL 0020000
#define IUTF8 0040000
#define OPOST 0000001
#define ONLCR 0000004
#define OCRNL 0000010
#define ONOCR 0000020
#define ONLRET 0000040
#define TABDLY 0014000
#define XTABS 0014000
#define B38400 0000017
#define CS8 0000060
#define CREAD 0000200
#define HUPCL 0002000
#define CLOCAL 0004000
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define TOSTOP 0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE 0004000
#define IEXTEN 0100000

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCSBRK 0x5409
#define TCXONC 0x540a
#define TCFLSH 0x540b
#define TIOCEXCL 0x540c
#define TIOCNXCL 0x540d
#define TIOCSCTTY 0x540e
#define TIOCGPGRP 0x540f
#define TIOCSPGRP 0x5410
#define TIOCOUTQ 0x5411
#define TIOCSTI 0x5412
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCMGET 0x5415
#define FIONREAD 0x541b
#define TIOCNOTTY 0x5422
#define TCSBRKP 0x5425
#define TIOCGSID 0x5429
#define TIOCGPTN 0x80045430
#define TIOCSPTLCK 0x40045431
#define TIOCGPTLCK 0x80045439
#define TIOCGPTPEER 0x5441
#define TIOCVHANGUP 0x5437

struct tty *console_tty;
static struct tty console;

static bool L(struct tty *t, uint32_t f) { return t->t.lflag & f; }
static bool I(struct tty *t, uint32_t f) { return t->t.iflag & f; }

static void tty_defaults(struct tty *t) {
  memset(&t->t, 0, sizeof(t->t));
  t->t.iflag = ICRNL | IXON | IUTF8;
  t->t.oflag = OPOST | ONLCR;
  t->t.cflag = B38400 | CS8 | CREAD | HUPCL;
  t->t.lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
  uint8_t cc[NCCS_K] = {3, 034, 0177, 025, 4, 0, 1, 0, 021, 023, 032, 0, 022, 017, 027, 026, 0};
  memcpy(t->t.cc, cc, sizeof(cc));
  t->ws.row = 24;
  t->ws.col = 80;
  waitq_init(&t->rq);
  waitq_init(&t->wq);
}

/* ---- output ---- */

static void out_raw(struct tty *t, const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c == '\n' || c == '\r')
      t->column = 0;
    else if (c == '\b') {
      if (t->column)
        t->column--;
    } else if (c == '\t')
      t->column = (t->column | 7) + 1;
    else if ((unsigned char)c >= 32)
      t->column++;
  }
  t->output(t, s, n);
}

/* OPOST processing of one buffer */
static void out_proc(struct tty *t, const char *s, size_t n) {
  if (!(t->t.oflag & OPOST)) {
    out_raw(t, s, n);
    return;
  }
  size_t start = 0;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    const char *rep = NULL;
    if (c == '\n' && (t->t.oflag & ONLCR))
      rep = "\r\n";
    else if (c == '\r' && (t->t.oflag & OCRNL))
      rep = "\n";
    else if (c == '\r' && (t->t.oflag & ONOCR) && t->column == 0)
      rep = "";
    if (rep) {
      out_raw(t, s + start, i - start);
      out_raw(t, rep, strlen(rep));
      start = i + 1;
    }
  }
  out_raw(t, s + start, n - start);
}

static void echo_char(struct tty *t, char c) {
  if (L(t, ECHOCTL) && (unsigned char)c < 32 && c != '\n' && c != '\t' && c != '\r') {
    char b[2] = {'^', (char)(c + '@')};
    out_proc(t, b, 2);
  } else if (L(t, ECHOCTL) && c == 0177) {
    out_proc(t, "^?", 2);
  } else
    out_proc(t, &c, 1);
}

/* ---- input ---- */

static void push_input(struct tty *t, const char *s, size_t n) {
  for (size_t i = 0; i < n && t->ilen < TTY_BUF; i++) {
    t->ibuf[(t->ihead + t->ilen) % TTY_BUF] = s[i];
    t->ilen++;
  }
}

static void push_line(struct tty *t, const char *s, size_t n) {
  if (t->nlines == TTY_LINES || t->ilen + n > TTY_BUF)
    return;
  push_input(t, s, n);
  t->lines[(t->lhead + t->nlines) % TTY_LINES] = (uint16_t)n;
  t->nlines++;
}

static void flush_input(struct tty *t) {
  t->ilen = 0;
  t->nlines = 0;
  t->linelen = 0;
}

static void flush_output(struct tty *t);

static void signal_pgrp(struct tty *t, int sig) {
  if (t->pgrp > 0)
    kill_pgrp(t->pgrp, sig, false);
}

static size_t char_width(struct tty *t, char c) {
  (void)t;
  if (c == '\t')
    return 0; /* handled by recomputation */
  if ((unsigned char)c < 32 || c == 0177)
    return L(t, ECHOCTL) ? 2 : 0;
  if (((unsigned char)c & 0xc0) == 0x80 && I(t, IUTF8))
    return 0; /* continuation byte */
  return 1;
}

static void erase_one(struct tty *t) {
  if (!t->linelen)
    return;
  /* a whole UTF-8 character */
  size_t n = 1;
  if (I(t, IUTF8))
    while (n < t->linelen && ((unsigned char)t->line[t->linelen - n] & 0xc0) == 0x80)
      n++;
  char c = t->line[t->linelen - n];
  t->linelen -= n;
  if (!L(t, ECHO))
    return;
  if (!L(t, ECHOE)) {
    echo_char(t, (char)t->t.cc[VERASE]);
    return;
  }
  size_t w;
  if (c == '\t') {
    /* the column the tab started at: recompute from the line */
    size_t col = 0;
    for (size_t i = 0; i < t->linelen; i++)
      col = t->line[i] == '\t' ? (col | 7) + 1 : col + char_width(t, t->line[i]);
    w = t->column > col ? t->column - col : 0;
    for (size_t i = 0; i < w; i++)
      out_raw(t, "\b", 1);
    return;
  }
  w = char_width(t, c);
  for (size_t i = 0; i < w; i++)
    out_raw(t, "\b \b", 3);
}

void tty_input(struct tty *t, char c) {
  random_mix((uint64_t)(unsigned char)c ^ rdtsc64());
  if (I(t, ISTRIP))
    c &= 0x7f;
  if (c == '\r') {
    if (I(t, IGNCR))
      return;
    if (I(t, ICRNL))
      c = '\n';
  } else if (c == '\n' && I(t, INLCR))
    c = '\r';
  uint8_t *cc = t->t.cc;
  if (I(t, IXON)) {
    if ((uint8_t)c == cc[VSTOP] && cc[VSTOP]) {
      t->stopped = true;
      return;
    }
    if ((uint8_t)c == cc[VSTART] && cc[VSTART]) {
      t->stopped = false;
      wake_up(&t->wq);
      return;
    }
    if (t->stopped && I(t, IXANY)) {
      t->stopped = false;
      wake_up(&t->wq);
    }
  }
  if (L(t, ISIG)) {
    int sig = 0;
    if ((uint8_t)c == cc[VINTR] && cc[VINTR])
      sig = SIGINT;
    else if ((uint8_t)c == cc[VQUIT] && cc[VQUIT])
      sig = SIGQUIT;
    else if ((uint8_t)c == cc[VSUSP] && cc[VSUSP])
      sig = SIGTSTP;
    if (sig) {
      if (!L(t, NOFLSH)) {
        flush_input(t);
        flush_output(t); /* as Linux: what the program wrote is discarded */
      }
      if (L(t, ECHO))
        echo_char(t, c);
      signal_pgrp(t, sig);
      wake_up(&t->rq);
      poll_wake();
      return;
    }
  }
  if (L(t, ICANON)) {
    if (L(t, IEXTEN) && (uint8_t)c == cc[VLNEXT] && cc[VLNEXT] && false)
      return;
    if ((uint8_t)c == cc[VERASE] && cc[VERASE]) {
      erase_one(t);
      return;
    }
    if (L(t, IEXTEN) && (uint8_t)c == cc[VWERASE] && cc[VWERASE]) {
      while (t->linelen && (t->line[t->linelen - 1] == ' ' || t->line[t->linelen - 1] == '\t'))
        erase_one(t);
      while (t->linelen && t->line[t->linelen - 1] != ' ' && t->line[t->linelen - 1] != '\t')
        erase_one(t);
      return;
    }
    if ((uint8_t)c == cc[VKILL] && cc[VKILL]) {
      if (L(t, ECHO) && L(t, ECHOKE) && L(t, ECHOE)) {
        while (t->linelen)
          erase_one(t);
      } else {
        t->linelen = 0;
        if (L(t, ECHO)) {
          echo_char(t, c);
          if (L(t, ECHOK))
            out_proc(t, "\n", 1);
        }
      }
      return;
    }
    if (L(t, IEXTEN) && (uint8_t)c == cc[VREPRINT] && cc[VREPRINT]) {
      if (L(t, ECHO)) {
        echo_char(t, c);
        out_proc(t, "\n", 1);
        out_proc(t, t->line, t->linelen);
      }
      return;
    }
    if ((uint8_t)c == cc[VEOF] && cc[VEOF]) {
      push_line(t, t->line, t->linelen);
      t->linelen = 0;
      wake_up(&t->rq);
      poll_wake();
      return;
    }
    bool eol = c == '\n' || ((uint8_t)c == cc[VEOL] && cc[VEOL]) ||
               (L(t, IEXTEN) && (uint8_t)c == cc[VEOL2] && cc[VEOL2]);
    if (t->linelen < TTY_BUF - 1 || eol)
      t->line[t->linelen++] = c;
    else
      return;
    if (L(t, ECHO) || (c == '\n' && L(t, ECHONL)))
      echo_char(t, c);
    if (eol) {
      push_line(t, t->line, t->linelen);
      t->linelen = 0;
      wake_up(&t->rq);
      poll_wake();
    }
    return;
  }
  if (t->ilen < TTY_BUF) {
    push_input(t, &c, 1);
    if (L(t, ECHO))
      echo_char(t, c);
  }
  wake_up(&t->rq);
  poll_wake();
}

/* ---- job control checks ---- */

static bool is_ctty(struct tty *t) { return cur_proc()->ctty == t && t->sid == cur_proc()->sid; }

static bool pgrp_orphaned_k(pid_t pgid) {
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->state != P_ALIVE || p->pgid != pgid)
      continue;
    struct proc *pp = p->parent;
    if (pp != p && pp->pgid != pgid && pp->sid == p->sid)
      return false;
  }
  return true;
}

/* a background process reading or writing its controlling terminal */
static int job_check(struct tty *t, int sig) {
  struct proc *p = cur_proc();
  if (!is_ctty(t) || t->pgrp <= 0 || p->pgid == t->pgrp)
    return 0;
  bool blocked_or_ignored = (current->blocked & SIGBIT(sig)) || p->sigact[sig - 1].handler == SIG_IGN;
  if (sig == SIGTTOU && blocked_or_ignored)
    return 0; /* write anyway */
  if (sig == SIGTTIN && blocked_or_ignored)
    return -EIO;
  if (pgrp_orphaned_k(p->pgid))
    return -EIO;
  kill_pgrp(p->pgid, sig, false);
  return -ERESTARTSYS;
}

/* ---- file operations ---- */

static struct tty *TT(struct file *f) { return f->priv; }

static size_t take(struct tty *t, char *dst, size_t n) {
  size_t k = 0;
  while (k < n && t->ilen) {
    dst[k++] = t->ibuf[t->ihead];
    t->ihead = (t->ihead + 1) % TTY_BUF;
    t->ilen--;
  }
  return k;
}

static ssize_t tty_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct tty *t = TT(f);
  int e = job_check(t, SIGTTIN);
  if (e)
    return e;
  if (n == 0)
    return 0;
  char tmp[TTY_BUF];
  uint64_t fl = irq_save();
  if (L(t, ICANON)) {
    while (t->nlines == 0) {
      if (t->hungup) {
        irq_restore(fl);
        return 0;
      }
      if (f->flags & O_NONBLOCK) {
        irq_restore(fl);
        return -EAGAIN;
      }
      if (sleep_on(&t->rq, 0, true) == -EINTR) {
        irq_restore(fl);
        return -ERESTARTSYS;
      }
    }
    size_t len = t->lines[t->lhead];
    size_t k = take(t, tmp, MIN(n, len));
    if (k == len) {
      t->lhead = (t->lhead + 1) % TTY_LINES;
      t->nlines--;
    } else
      t->lines[t->lhead] = (uint16_t)(len - k);
    irq_restore(fl);
    if (copy_to_user(buf, tmp, k))
      return -EFAULT;
    return (ssize_t)k;
  }
  unsigned vmin = t->t.cc[VMIN], vtime = t->t.cc[VTIME];
  size_t want = MIN(n, (size_t)(vmin ? vmin : 1));
  uint64_t deadline = vtime ? monotonic_ns() + (uint64_t)vtime * 100000000UL : 0;
  size_t got = 0;
  for (;;) {
    got += take(t, tmp + got, MIN(n, sizeof(tmp)) - got);
    if (got >= want || (got && vmin == 0))
      break;
    if (vmin == 0 && vtime == 0)
      break;
    if (t->hungup)
      break;
    if (f->flags & O_NONBLOCK) {
      if (!got) {
        irq_restore(fl);
        return -EAGAIN;
      }
      break;
    }
    int r = sleep_on(&t->rq, deadline, true);
    if (r == -EINTR) {
      if (got)
        break;
      irq_restore(fl);
      return -ERESTARTSYS;
    }
    if (r == -ETIMEDOUT)
      break;
    if (vtime && vmin && got) /* inter-byte timer restarts */
      deadline = monotonic_ns() + (uint64_t)vtime * 100000000UL;
  }
  irq_restore(fl);
  if (copy_to_user(buf, tmp, got))
    return -EFAULT;
  return (ssize_t)got;
}

static size_t room_for(struct tty *t);
static bool pty_master_gone(struct tty *t);

static ssize_t tty_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct tty *t = TT(f);
  if (L(t, TOSTOP)) {
    int e = job_check(t, SIGTTOU);
    if (e)
      return e;
  }
  if (t->hungup)
    return -EIO;
  char tmp[512];
  size_t done = 0;
  while (done < n) {
    uint64_t fl = irq_save();
    while (t->stopped) {
      if (sleep_on(&t->wq, 0, true) == -EINTR) {
        irq_restore(fl);
        return done ? (ssize_t)done : -ERESTARTSYS;
      }
    }
    irq_restore(fl);
    size_t c = MIN(n - done, sizeof(tmp));
    if (copy_from_user(tmp, buf + done, c))
      return done ? (ssize_t)done : -EFAULT;
    fl = irq_save();
    /* a pseudo-terminal: wait for room in the master's buffer (OPOST
       may double the size) */
    while (t->pty && room_for(t) < 2 * c) {
      if (pty_master_gone(t)) {
        irq_restore(fl);
        return done ? (ssize_t)done : -EIO;
      }
      if (f->flags & O_NONBLOCK) {
        irq_restore(fl);
        return done ? (ssize_t)done : -EAGAIN;
      }
      if (sleep_on(&t->wq, 0, true) == -EINTR) {
        irq_restore(fl);
        return done ? (ssize_t)done : -ERESTARTSYS;
      }
    }
    out_proc(t, tmp, c);
    irq_restore(fl);
    done += c;
  }
  return (ssize_t)done;
}

static unsigned tty_poll(struct file *f) {
  struct tty *t = TT(f);
  unsigned m = (!t->pty || room_for(t) >= 1024) ? POLLOUT | POLLWRNORM : 0;
  if (L(t, ICANON) ? t->nlines > 0 : t->ilen > 0)
    m |= POLLIN | POLLRDNORM;
  /* hung up: readable (end-of-file), not writable (POLLHUP and POLLOUT
     exclude each other), and writes fail (EIO): POLLERR */
  if (t->hungup)
    m = (m & ~(unsigned)(POLLOUT | POLLWRNORM)) | POLLHUP | POLLIN | POLLERR;
  return m;
}

static void set_ctty(struct tty *t, struct proc *p) {
  p->ctty = t;
  t->sid = p->sid;
  t->pgrp = p->pgid;
}

static const struct file_ops pts_fops;

void tty_maybe_set_ctty(struct file *f) {
  if (!f->fops || !f->priv)
    return;
  extern const struct file_ops tty_fops_ext;
  struct proc *p = cur_proc();
  struct tty *t = TT(f);
  if (!t || (f->fops != &tty_fops_ext && f->fops != &pts_fops))
    return;
  /* as Linux: /dev/console never becomes a controlling terminal */
  if (f->ino && f->ino->rdev == MKDEV(5, 1))
    return;
  if (p->sid == p->pid && !p->ctty && t->sid == 0)
    set_ctty(t, p);
}

void tty_session_hangup(struct tty *t, pid_t sid) {
  if (t->sid != sid)
    return;
  if (t->pgrp > 0) {
    kill_pgrp(t->pgrp, SIGHUP, false);
    kill_pgrp(t->pgrp, SIGCONT, false);
  }
  t->sid = 0;
  t->pgrp = 0;
  list_for_each(q, &all_procs) {
    struct proc *p = list_entry(q, struct proc, all_link);
    if (p->sid == sid && p->ctty == t)
      p->ctty = NULL;
  }
}

static long tty_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  struct tty *t = TT(f);
  struct proc *p = cur_proc();
  void *u = (void *)arg;
  switch (cmd) {
  case TCGETS:
    return copy_to_user(u, &t->t, sizeof(t->t)) ? -EFAULT : 0;
  case TCSETS:
  case TCSETSW:
  case TCSETSF: {
    struct ktermios nt;
    if (copy_from_user(&nt, u, sizeof(nt)))
      return -EFAULT;
    int e = job_check(t, SIGTTOU);
    if (e)
      return e;
    uint64_t fl = irq_save();
    if (cmd == TCSETSF)
      flush_input(t);
    bool was_canon = L(t, ICANON);
    t->t = nt;
    if (was_canon && !L(t, ICANON) && t->linelen) {
      /* the partial line becomes readable */
      push_input(t, t->line, t->linelen);
      t->linelen = 0;
    } else if (!was_canon && L(t, ICANON) && t->ilen) {
      t->nlines = 0;
      t->lines[t->lhead] = 0;
      push_line(t, "", 0);
      t->lines[(t->lhead + t->nlines - 1) % TTY_LINES] = (uint16_t)t->ilen;
    }
    wake_up(&t->rq);
    wake_up(&t->wq);
    poll_wake();
    irq_restore(fl);
    return 0;
  }
  case TCSBRK:
  case TCSBRKP:
    return 0;
  case TCXONC:
    if (arg == 0)
      t->stopped = true;
    else if (arg == 1) {
      t->stopped = false;
      wake_up(&t->wq);
    } else if (arg > 3)
      return -EINVAL;
    return 0;
  case TCFLSH:
    if (arg > 2)
      return -EINVAL;
    if (arg == 0 || arg == 2)
      flush_input(t);
    if (arg == 1 || arg == 2)
      flush_output(t);
    return 0;
  case TIOCGWINSZ:
    return copy_to_user(u, &t->ws, sizeof(t->ws)) ? -EFAULT : 0;
  case TIOCSWINSZ: {
    struct winsize w;
    if (copy_from_user(&w, u, sizeof(w)))
      return -EFAULT;
    bool changed = memcmp(&w, &t->ws, sizeof(w)) != 0;
    t->ws = w;
    if (changed)
      signal_pgrp(t, SIGWINCH);
    return 0;
  }
  case TIOCGPGRP:
    if (!is_ctty(t))
      return -ENOTTY;
    {
      int32_t g = t->pgrp;
      return copy_to_user(u, &g, 4) ? -EFAULT : 0;
    }
  case TIOCSPGRP: {
    int32_t g;
    if (!is_ctty(t))
      return -ENOTTY;
    if (copy_from_user(&g, u, 4))
      return -EFAULT;
    if (g < 0)
      return -EINVAL;
    int e = job_check(t, SIGTTOU);
    if (e)
      return e;
    bool ok = false;
    list_for_each(q, &all_procs) {
      struct proc *o = list_entry(q, struct proc, all_link);
      if (o->state != P_DEAD && o->pgid == g) {   /* (zombies are members) */
        if (o->sid != p->sid)
          return -EPERM;
        ok = true;
      }
    }
    if (!ok)
      return -EPERM;
    t->pgrp = g;
    return 0;
  }
  case TIOCGSID:
    if (!is_ctty(t) || t->sid <= 0)
      return -ENOTTY;
    {
      int32_t s = t->sid;
      return copy_to_user(u, &s, 4) ? -EFAULT : 0;
    }
  case TIOCSCTTY:
    if (p->sid != p->pid || p->ctty) {
      if (p->ctty == t)
        return 0;
      return -EPERM;
    }
    if (t->sid && t->sid != p->sid) {
      if (arg != 1 || !capable())
        return -EPERM;
      tty_session_hangup(t, t->sid);
    }
    set_ctty(t, p);
    return 0;
  case TIOCNOTTY:
    if (!is_ctty(t))
      return -ENOTTY;
    if (p->sid == p->pid)
      tty_session_hangup(t, p->sid);
    else
      p->ctty = NULL;
    return 0;
  case FIONREAD:
  case TIOCOUTQ: {
    int32_t n = cmd == FIONREAD ? (int32_t)(L(t, ICANON) ? (t->nlines ? t->ilen : 0) : t->ilen) : 0;
    return copy_to_user(u, &n, 4) ? -EFAULT : 0;
  }
  case TIOCSTI: {
    if (!capable() && !is_ctty(t))
      return -EPERM;
    char c;
    if (copy_from_user(&c, u, 1))
      return -EFAULT;
    uint64_t fl = irq_save();
    tty_input(t, c);
    irq_restore(fl);
    return 0;
  }
  case TIOCEXCL:
  case TIOCNXCL:
    return 0;
  case TIOCMGET: {
    int32_t m = 0x002 | 0x004 | 0x100; /* DTR RTS DSR */
    return copy_to_user(u, &m, 4) ? -EFAULT : 0;
  }
  case TIOCVHANGUP:
    if (!capable())
      return -EPERM;
    tty_session_hangup(t, t->sid);
    return 0;
  }
  return -ENOTTY;
}

static void tty_release(struct file *f) {
  struct tty *t = TT(f);
  if (t)
    t->opens--;
}

const struct file_ops tty_fops_ext = {.read = tty_read,
                                      .write = tty_write,
                                      .poll = tty_poll,
                                      .ioctl = tty_ioctl,
                                      .release = tty_release};

/* ---- the console: COM1 and the VGA screen ---- */

static void console_out(struct tty *t, const char *s, size_t n) {
  (void)t;
  serial_write(s, n);
  vga_write(s, n);
}

static int console_open(struct inode *ino, struct file *f) {
  (void)ino;
  f->priv = console_tty;
  f->fops = &tty_fops_ext;
  console_tty->opens++;
  return 0;
}

/* /dev/tty: the controlling terminal */
static int devtty_open(struct inode *ino, struct file *f) {
  (void)ino;
  struct tty *t = cur_proc()->ctty;
  if (!t)
    return -ENXIO;
  f->priv = t;
  f->fops = &tty_fops_ext;
  t->opens++;
  return 0;
}

/* ---- pseudo-terminals ---- */

#define NPTY 256

struct pty {
  int index;
  struct tty slave;
  char obuf[TTY_BUF]; /* slave output, for the master to read */
  size_t ohead, olen;
  struct waitq mrq, mwq;
  bool locked, master_open;
  int slave_opens;
  bool slave_ever_opened;
  uid_t uid;
};

static struct pty *ptys[NPTY];

static void pty_slave_out(struct tty *t, const char *s, size_t n) {
  struct pty *p = t->pty;
  for (size_t i = 0; i < n; i++) {
    if (p->olen == TTY_BUF)
      break; /* the master is not reading: drop (Linux would block) */
    p->obuf[(p->ohead + p->olen) % TTY_BUF] = s[i];
    p->olen++;
  }
  wake_up(&p->mrq);
  poll_wake();
}

static ssize_t ptm_read(struct file *f, char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct pty *p = f->priv;
  char tmp[TTY_BUF];
  uint64_t fl = irq_save();
  while (p->olen == 0) {
    if (p->slave_ever_opened && p->slave_opens == 0) {
      irq_restore(fl);
      return -EIO;
    }
    if (f->flags & O_NONBLOCK) {
      irq_restore(fl);
      return -EAGAIN;
    }
    if (sleep_on(&p->mrq, 0, true) == -EINTR) {
      irq_restore(fl);
      return -ERESTARTSYS;
    }
  }
  size_t k = 0;
  while (k < n && k < sizeof(tmp) && p->olen) {
    tmp[k++] = p->obuf[p->ohead];
    p->ohead = (p->ohead + 1) % TTY_BUF;
    p->olen--;
  }
  irq_restore(fl);
  wake_up(&p->slave.wq);
  if (copy_to_user(buf, tmp, k))
    return -EFAULT;
  return (ssize_t)k;
}

static ssize_t ptm_write(struct file *f, const char *buf, size_t n, off_t *pos) {
  (void)pos;
  struct pty *p = f->priv;
  char tmp[256];
  size_t done = 0;
  while (done < n) {
    size_t c = MIN(n - done, sizeof(tmp));
    if (copy_from_user(tmp, buf + done, c))
      return done ? (ssize_t)done : -EFAULT;
    uint64_t fl = irq_save();
    /* wait while the slave's input is full */
    while (p->slave.ilen >= TTY_BUF - 256 || (L(&p->slave, ICANON) && p->slave.linelen >= TTY_BUF - 256 && false)) {
      if (f->flags & O_NONBLOCK) {
        irq_restore(fl);
        return done ? (ssize_t)done : -EAGAIN;
      }
      if (sleep_on(&p->mwq, 0, true) == -EINTR) {
        irq_restore(fl);
        return done ? (ssize_t)done : -ERESTARTSYS;
      }
    }
    for (size_t i = 0; i < c; i++)
      tty_input(&p->slave, tmp[i]);
    irq_restore(fl);
    done += c;
  }
  return (ssize_t)done;
}

static unsigned ptm_poll(struct file *f) {
  struct pty *p = f->priv;
  unsigned m = 0;
  if (p->olen)
    m |= POLLIN | POLLRDNORM;
  if (p->slave.ilen < TTY_BUF - 256)
    m |= POLLOUT | POLLWRNORM;
  if (p->slave_ever_opened && p->slave_opens == 0)
    m |= POLLHUP;
  return m;
}

static void pty_maybe_free(struct pty *p) {
  if (!p->master_open && p->slave_opens == 0) {
    /* no process may keep it as its controlling terminal */
    list_for_each(q, &all_procs) {
      struct proc *pr = list_entry(q, struct proc, all_link);
      if (pr->ctty == &p->slave)
        pr->ctty = NULL;
    }
    ptys[p->index] = NULL;
    kfree(p);
  }
}

static size_t room_for(struct tty *t) { return TTY_BUF - t->pty->olen; }
static bool pty_master_gone(struct tty *t) { return !t->pty->master_open; }

static void flush_output(struct tty *t) {
  if (t->pty) {
    t->pty->olen = 0;
    wake_up(&t->wq);
  }
}

static void ptm_release(struct file *f) {
  struct pty *p = f->priv;
  p->master_open = false;
  /* hang up the slave */
  p->slave.hungup = true;
  if (p->slave.sid)
    tty_session_hangup(&p->slave, p->slave.sid);
  wake_up(&p->slave.rq);
  wake_up(&p->slave.wq);
  poll_wake();
  char name[16];
  snprintf(name, sizeof(name), "pts/%d", p->index);
  devtmpfs_remove(name);
  pty_maybe_free(p);
}

static long ptm_ioctl(struct file *f, unsigned cmd, uint64_t arg) {
  struct pty *p = f->priv;
  switch (cmd) {
  case TIOCGPTN: {
    uint32_t n = (uint32_t)p->index;
    return copy_to_user((void *)arg, &n, 4) ? -EFAULT : 0;
  }
  case TIOCSPTLCK: {
    int32_t v;
    if (copy_from_user(&v, (void *)arg, 4))
      return -EFAULT;
    p->locked = v != 0;
    return 0;
  }
  case TIOCGPTLCK: {
    int32_t v = p->locked;
    return copy_to_user((void *)arg, &v, 4) ? -EFAULT : 0;
  }
  case FIONREAD: {
    int32_t v = (int32_t)p->olen;
    return copy_to_user((void *)arg, &v, 4) ? -EFAULT : 0;
  }
  case TIOCSCTTY:
  case TIOCGPGRP:
  case TIOCSPGRP:
  case TIOCGSID:
  case TIOCNOTTY:
    return -ENOTTY;
  }
  /* termios and window size act on the slave */
  struct file sf = *f;
  sf.priv = &p->slave;
  return tty_ioctl(&sf, cmd, arg);
}

static const struct file_ops ptm_fops = {
    .read = ptm_read, .write = ptm_write, .poll = ptm_poll, .ioctl = ptm_ioctl, .release = ptm_release};

static int ptmx_open(struct inode *ino, struct file *f) {
  (void)ino;
  int i;
  for (i = 0; i < NPTY && ptys[i]; i++)
    ;
  if (i == NPTY)
    return -ENOSPC;
  struct pty *p = kzalloc(sizeof(*p));
  if (!p)
    return -ENOMEM;
  p->index = i;
  tty_defaults(&p->slave);
  /* a new pseudoterminal has no size until its controller sets one (as on
     Linux and the BSDs); the console is 24x80 */
  p->slave.ws.row = p->slave.ws.col = 0;
  p->slave.t.cflag = B38400 | CS8 | CREAD; /* as Linux's pty driver */
  snprintf(p->slave.name, sizeof(p->slave.name), "pts/%d", i);
  p->slave.output = pty_slave_out;
  p->slave.pty = p;
  p->slave.index = i;
  waitq_init(&p->mrq);
  waitq_init(&p->mwq);
  p->locked = true;
  p->master_open = true;
  p->uid = cur_proc()->cred.uid;
  ptys[i] = p;
  f->priv = p;
  f->fops = &ptm_fops;
  char name[16];
  snprintf(name, sizeof(name), "pts/%d", i);
  struct inode *n = devtmpfs_add(name, S_IFCHR | 0620, MKDEV(136, i));
  if (n && !IS_ERR(n)) {
    n->uid = p->uid;
    n->gid = 5; /* tty */
  }
  return 0;
}

static void pts_release(struct file *f) {
  struct tty *t = f->priv;
  struct pty *p = t->pty;
  t->opens--;
  p->slave_opens--;
  wake_up(&p->mrq);
  poll_wake();
  pty_maybe_free(p);
}

static const struct file_ops pts_fops = {.read = tty_read,
                                         .write = tty_write,
                                         .poll = tty_poll,
                                         .ioctl = tty_ioctl,
                                         .release = pts_release};

static int pts_open(struct inode *ino, struct file *f) {
  unsigned i = MINOR(ino->rdev);
  if (i >= NPTY || !ptys[i] || !ptys[i]->master_open)
    return -EIO;
  struct pty *p = ptys[i];
  if (p->locked)
    return -EIO;
  p->slave_opens++;
  p->slave_ever_opened = true;
  p->slave.opens++;
  p->slave.hungup = false;
  f->priv = &p->slave;
  f->fops = &pts_fops;
  /* (the controlling terminal rule applies to pty slaves too: in
     tty_maybe_set_ctty, which do_open calls without O_NOCTTY, which the
     file's flags do not keep) */
  return 0;
}

/* character devices: major 4 (ttyS, tty1), 5 (tty, console, ptmx), 136 */
static int tty4_open(struct inode *ino, struct file *f) { return console_open(ino, f); }

static int tty5_open(struct inode *ino, struct file *f) {
  switch (MINOR(ino->rdev)) {
  case 0:
    return devtty_open(ino, f);
  case 1:
    return console_open(ino, f);
  case 2:
    return ptmx_open(ino, f);
  }
  return -ENXIO;
}

static const struct chrdev tty4 = {"tty", tty4_open};
static const struct chrdev tty5 = {"tty5", tty5_open};
static const struct chrdev pts = {"pts", pts_open};

void tty_init(void) {
  tty_defaults(&console);
  strcpy(console.name, "console");
  console.output = console_out;
  console_tty = &console;
  chrdev_register(4, &tty4);
  chrdev_register(5, &tty5);
  chrdev_register(136, &pts);
}

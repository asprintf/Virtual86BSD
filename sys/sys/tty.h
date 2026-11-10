/* tty.h - terminals: line discipline, console, pseudo-terminals. */
#ifndef _SYS_TTY_H_
#define _SYS_TTY_H_

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/termios.h>

struct file;

#define TTY_BUF 4096
#define TTY_LINES 256

struct pty;

struct tty {
  int index;
  char name[16];
  struct ktermios t;
  struct winsize ws;
  pid_t pgrp, sid;
  /* input ready for read */
  char ibuf[TTY_BUF];
  size_t ihead, ilen;
  uint16_t lines[TTY_LINES]; /* canonical: lengths of complete lines */
  int lhead, nlines;
  /* the line being edited (canonical mode) */
  char line[TTY_BUF];
  size_t linelen;
  size_t column; /* output column, for erasing tabs */
  bool stopped, hungup;
  int opens;
  struct waitq rq, wq;
  void (*output)(struct tty *t, const char *s, size_t n); /* raw output */
  struct pty *pty;
};

extern struct tty *console_tty;
void tty_init(void);
void tty_input(struct tty *t, char c);
void tty_maybe_set_ctty(struct file *f);
void tty_session_hangup(struct tty *t, pid_t sid);
void serial_enable_irq(void);

#endif

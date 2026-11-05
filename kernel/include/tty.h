/* tty.h - terminals: line discipline, console, pseudo-terminals. */
#ifndef TTY_H
#define TTY_H

#include "kernel.h"
#include "proc.h"

struct file;

#define NCCS_K 19
struct ktermios {
  uint32_t iflag, oflag, cflag, lflag;
  uint8_t line;
  uint8_t cc[NCCS_K];
};

struct winsize {
  uint16_t row, col, xpixel, ypixel;
};

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

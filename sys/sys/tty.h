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

#endif

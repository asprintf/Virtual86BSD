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

/* subr_prf.c - kernel messages: to the console devices and a log buffer that
   syslog(2)/dmesg can read. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <machine/cpu.h>

#define LOG_SIZE MSGBUFSIZE
static char logbuf[LOG_SIZE];
static uint64_t log_end; /* total bytes ever written */

/* /dev/klog: its reader has taken everything before klog_rd.  The wait
   queue exists once someone opened the device (klog_users), and the
   messages are not announced from inside the announcement itself. */
struct waitq klog_wq;
static uint64_t klog_rd;
static int klog_users;
static bool klog_waking;

static void log_write(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++)
    logbuf[log_end++ % LOG_SIZE] = s[i];
}

size_t log_read(char *dst, size_t n) {
  uint64_t start = log_end > LOG_SIZE ? log_end - LOG_SIZE : 0;
  size_t have = (size_t)(log_end - start);
  if (n > have)
    n = have;
  start = log_end - n;
  for (size_t i = 0; i < n; i++)
    dst[i] = logbuf[(start + i) % LOG_SIZE];
  return n;
}

/* the text of the buffer, from the start of a line */
size_t msgbuf_read(char *dst, size_t n) {
  uint64_t f = irq_save();
  uint64_t start = log_end > LOG_SIZE ? log_end - LOG_SIZE : 0;
  if (start) {
    /* the buffer has wrapped: its oldest line is cut at the start, and there
       is no telling where it began (the byte before is gone), so go on from
       the next line (a buffer that is one long line is left as it is) */
    uint64_t p = start;
    while (p < log_end && logbuf[p % LOG_SIZE] != '\n')
      p++;
    if (p < log_end)
      start = p + 1;
  }
  size_t have = (size_t)(log_end - start), k = have < n ? have : n;
  start = log_end - k;
  for (size_t i = 0; i < k; i++)
    dst[i] = logbuf[(start + i) % LOG_SIZE];
  irq_restore(f);
  return k;
}

void klog_open(void) {
  uint64_t f = irq_save();
  if (!klog_users)
    waitq_init(&klog_wq);
  klog_users++;
  irq_restore(f);
}

void klog_close(void) {
  uint64_t f = irq_save();
  klog_users--;
  irq_restore(f);
}

bool klog_avail(void) { return log_end > klog_rd; }

/* up to n bytes of what the reader has not taken (what the buffer lost
   meanwhile is gone: the reader goes on from the oldest byte left) */
size_t klog_peek(char *dst, size_t n) {
  uint64_t f = irq_save();
  if (log_end - klog_rd > LOG_SIZE)
    klog_rd = log_end - LOG_SIZE;
  size_t have = (size_t)(log_end - klog_rd), k = have < n ? have : n;
  for (size_t i = 0; i < k; i++)
    dst[i] = logbuf[(klog_rd + i) % LOG_SIZE];
  irq_restore(f);
  return k;
}

void klog_consume(size_t n) {
  uint64_t f = irq_save();
  klog_rd += n;
  if (klog_rd > log_end)
    klog_rd = log_end;
  irq_restore(f);
}

void printf(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > (int)sizeof(buf) - 1)
    n = sizeof(buf) - 1;
  uint64_t f = irq_save();
  log_write(buf, n);
  console_write(buf, n);
  if (klog_users && !klog_waking) {
    klog_waking = true;
    wake_up(&klog_wq);
    poll_wake();
    klog_waking = false;
  }
  irq_restore(f);
}

void panic(const char *fmt, ...) {
  char buf[512];
  cli();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("\npanic: %s\nThe operating system has halted.\n", buf);
  for (;;)
    hlt();
}

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
#include <machine/cpu.h>

#define LOG_SIZE 65536
static char logbuf[LOG_SIZE];
static uint64_t log_end; /* total bytes ever written */

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

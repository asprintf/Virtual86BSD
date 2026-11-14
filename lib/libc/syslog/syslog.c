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

/* <syslog.h>: messages as RFC 3164 lines, "<PRI>Mmm dd hh:mm:ss
 * TAG[PID]: text", on a datagram socket connected to /dev/log; with
 * LOG_CONS, on /dev/console when that fails.  %m stands for
 * strerror(errno).  The tag defaults to the program's name. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include "lock.h"

#ifndef LOG_PERROR
#define LOG_PERROR 0x20     /* <syslog.h> declares it outside the POSIX namespace */
#endif

extern char *__progname;    /* the basename of argv[0] (__libc_start_main) */

static volatile int lock;
static char tag[64];
static int opts, facility = LOG_USER, mask = 0xff, fd = -1;

/* without a syslogd (no /dev/log) messages are dropped; errno is kept:
   openlog() and syslog() have no errors */
static void opensock(void)
{
	static const struct sockaddr_un log = {AF_UNIX, "/dev/log"};
	int e = errno;
	if (fd < 0)
		fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd >= 0 && connect(fd, (const struct sockaddr *)&log, sizeof log) < 0) {
		close(fd);
		fd = -1;
	}
	errno = e;
}

void openlog(const char *ident, int option, int fac)
{
	LOCK(&lock);
	if (ident) {
		strncpy(tag, ident, sizeof tag - 1);
		tag[sizeof tag - 1] = 0;
	} else
		tag[0] = 0;
	opts = option;
	if (fac && !(fac & ~(0x3ff & ~7)))
		facility = fac;
	if (option & LOG_NDELAY)
		opensock();
	UNLOCK(&lock);
}

void closelog(void)
{
	LOCK(&lock);
	if (fd >= 0)
		close(fd);
	fd = -1;
	UNLOCK(&lock);
}

int setlogmask(int m)
{
	int old;
	LOCK(&lock);
	old = mask;
	if (m)
		mask = m;
	UNLOCK(&lock);
	return old;
}

void __vsyslog(int pri, const char *fmt, va_list ap)
{
	char fmt2[512], msg[1024], hdr[128], line[1200];
	const char *t = tag;
	int e = errno, hlen, n, i, j;
	time_t now;
	struct tm tm;
	static const char mon[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	if (!(mask & LOG_MASK(pri & 7)) || (pri & ~0x3ff))
		return;
	/* %m, then the rest of the format */
	for (i = j = 0; fmt[i] && j < (int)sizeof fmt2 - 1; i++) {
		if (fmt[i] == '%' && fmt[i + 1] == 'm') {
			const char *s = strerror(e);
			for (; *s && j < (int)sizeof fmt2 - 1; s++) {
				if (*s == '%' && j < (int)sizeof fmt2 - 2)
					fmt2[j++] = '%';
				fmt2[j++] = *s;
			}
			i++;
		} else if (fmt[i] == '%' && fmt[i + 1] == '%') {
			fmt2[j++] = '%';
			if (j < (int)sizeof fmt2 - 1)
				fmt2[j++] = '%';
			i++;
		} else
			fmt2[j++] = fmt[i];
	}
	fmt2[j] = 0;
	vsnprintf(msg, sizeof msg, fmt2, ap);

	if (!(pri & ~7))
		pri |= facility;
	time(&now);
	localtime_r(&now, &tm);
	if (!*t)
		t = __progname ? __progname : "";
	hlen = snprintf(hdr, sizeof hdr, "<%d>%s %2d %02d:%02d:%02d ", pri, mon[tm.tm_mon], tm.tm_mday,
	                tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (opts & LOG_PID)
		n = snprintf(line, sizeof line, "%s%s[%d]: %s", hdr, t, (int)getpid(), msg);
	else
		n = snprintf(line, sizeof line, "%s%s: %s", hdr, t, msg);
	if (n >= (int)sizeof line)
		n = sizeof line - 1;

	LOCK(&lock);
	if (fd < 0)
		opensock();
	if (fd < 0 || send(fd, line, (size_t)n, MSG_NOSIGNAL | MSG_DONTWAIT) < 0) {
		/* never wait for a syslogd that does not keep up: the message is
		   lost (the console has it with LOG_CONS); else the socket may
		   have been reconnected (syslogd restarted) */
		if (fd >= 0 && errno != EAGAIN) {
			close(fd);
			fd = -1;
			opensock();
		}
		if ((fd < 0 || send(fd, line, (size_t)n, MSG_NOSIGNAL | MSG_DONTWAIT) < 0) && (opts & LOG_CONS)) {
			int c = open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC);
			if (c >= 0) {
				dprintf(c, "%s\r\n", line + strcspn(line, ">") + 1);
				close(c);
			}
		}
	}
	if (opts & LOG_PERROR)
		dprintf(2, "%s\n", line + hlen);
	UNLOCK(&lock);
	errno = e;
}

void syslog(int pri, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__vsyslog(pri, fmt, ap);
	va_end(ap);
}

extern __typeof(__vsyslog) vsyslog __attribute__((weak, alias("__vsyslog")));

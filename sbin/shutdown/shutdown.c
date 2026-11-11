/*
 * Copyright (c) 2026, asprintf
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

/* shutdown - bring the system down at a time, warning the users.
 *
 *   shutdown [-hpr] time [warning-message ...]
 *
 * time is `now' or `+minutes'.  The system is brought down by init(8): -r
 * reboots (SIGINT), -h halts (SIGUSR1), -p powers off (SIGUSR2), and with
 * none of them every process is ended and a shell is left on the console
 * (SIGTERM, single-user mode).  Root only.
 *
 * At once /etc/nologin is made (it holds the time and the message: login
 * turns users away with it; /etc/rc removes it at the next boot) and the users
 * logged in (the lines of utmpx) are told, again every ten minutes (every
 * half hour when it is far), at 5, 3, 2 and 1 minutes and when the time has
 * come.  With a delay shutdown goes on in the background (it prints its pid, and lets
 * go of its terminal);
 * SIGINT or SIGTERM to it (or ^C if it was not sent there) cancels it, takes
 * /etc/nologin away and tells the users. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#define PATH_NOLOGIN "/etc/nologin"

static char message[512];
static int sig_to_init = SIGTERM;
static const char *what = "go down";

static void usage(void)
{
	fprintf(stderr, "usage: shutdown [-hpr] time [warning-message ...]\n");
	exit(1);
}

/* tell every terminal that has a user on it (the lines of utmpx) */
static void wall(const char *text)
{
	struct utmpx *u;
	char dev[sizeof u->ut_line + 8];

	setutxent();
	while ((u = getutxent())) {
		int fd;
		size_t l;
		if (u->ut_type != USER_PROCESS || !u->ut_line[0])
			continue;
		l = strnlen(u->ut_line, sizeof u->ut_line);
		if (memchr(u->ut_line, '/', l) && strncmp(u->ut_line, "pts/", 4))
			continue;
		snprintf(dev, sizeof dev, "/dev/%.*s", (int)l, u->ut_line);
		fd = open(dev, O_WRONLY | O_NONBLOCK | O_NOCTTY);
		if (fd < 0)
			continue;
		(void)!write(fd, text, strlen(text));
		close(fd);
	}
	endutxent();
}

static void announce(time_t when, int minutes)
{
	char text[1024], clock[64];
	struct tm *tm = localtime(&when);

	if (tm)
		strftime(clock, sizeof clock, "%a %b %e %H:%M", tm);
	else
		clock[0] = 0;
	if (minutes <= 0)
		snprintf(text, sizeof text, "\r\n\a\aSystem shutdown time has arrived\a\a\r\n\r\n");
	else
		snprintf(text, sizeof text, "\r\n\a\aSystem going down at %s (in %d minute%s)\r\n%s%s\r\n", clock, minutes,
		    minutes == 1 ? "" : "s", message, message[0] ? "\r\n" : "");
	wall(text);
}

static void write_nologin(time_t when)
{
	char clock[64];
	struct tm *tm = localtime(&when);
	FILE *f;

	if (tm)
		strftime(clock, sizeof clock, "%a %b %e %H:%M:%S %Y", tm);
	else
		clock[0] = 0;
	f = fopen(PATH_NOLOGIN, "w");
	if (!f) {
		fprintf(stderr, "shutdown: %s: %s\n", PATH_NOLOGIN, strerror(errno));
		return;
	}
	fprintf(f, "\n\nSystem going down at %s\n\n%s%s", clock, message, message[0] ? "\n\n" : "");
	fclose(f);
	chmod(PATH_NOLOGIN, 0644);
}

static void cancelled(int sig)
{
	static const char m[] = "\r\n\a\aShutdown cancelled\r\n";
	(void)sig;
	unlink(PATH_NOLOGIN);
	wall(m);
	_exit(1);
}

int main(int argc, char *argv[])
{
	int ch, i;
	char *when, *end;
	long minutes = 0;
	time_t at, now;

	while ((ch = getopt(argc, argv, "hpr")) != -1) {
		switch (ch) {
		case 'h': sig_to_init = SIGUSR1; what = "halt"; break;
		case 'p': sig_to_init = SIGUSR2; what = "power off"; break;
		case 'r': sig_to_init = SIGINT; what = "reboot"; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();
	when = argv[0];
	if (!strcmp(when, "now")) {
		minutes = 0;
	} else if (when[0] == '+' && when[1]) {
		errno = 0;
		minutes = strtol(when + 1, &end, 10);
		if (*end || errno || minutes < 0 || minutes > 7 * 24 * 60)
			usage();
	} else {
		usage();
	}
	for (i = 1; i < argc; i++) {
		if (strlen(message) + strlen(argv[i]) + 2 >= sizeof message)
			break;
		if (i > 1)
			strcat(message, " ");
		strcat(message, argv[i]);
	}
	if (geteuid() != 0) {
		fprintf(stderr, "shutdown: you must be root\n");
		return 1;
	}
	now = time(NULL);
	at = now + minutes * 60;
	write_nologin(at);
	if (minutes > 0) {
		pid_t pid = fork();
		if (pid < 0) {
			fprintf(stderr, "shutdown: fork: %s\n", strerror(errno));
			unlink(PATH_NOLOGIN);
			return 1;
		}
		if (pid > 0) {
			printf("shutdown: [pid %d]\n", (int)pid);
			return 0;
		}
		setsid();
		/* let go of the caller's terminal and pipes (a caller that waits for the
		   output of shutdown must not wait for the shutdown) */
		{
			int fd = open("/dev/null", O_RDWR);
			if (fd >= 0) {
				dup2(fd, 0);
				dup2(fd, 1);
				dup2(fd, 2);
				if (fd > 2)
					close(fd);
			}
		}
	}
	signal(SIGINT, cancelled);
	signal(SIGTERM, cancelled);
	signal(SIGHUP, SIG_IGN);
	for (;;) {
		long left;
		now = time(NULL);
		left = (long)(at - now);
		if (left <= 0)
			break;
		{
			long m = (left + 59) / 60, next;
			announce(at, (int)m);
			/* the next warning: every 30 minutes beyond an hour, every 10 beyond 15,
			   then at 5, 3, 2 and 1 minutes, then the time itself */
			if (m > 60) next = m - (m % 30 ? m % 30 : 30);
			else if (m > 15) next = m - (m % 10 ? m % 10 : 10);
			else if (m > 5) next = 5;
			else if (m > 3) next = 3;
			else if (m > 2) next = 2;
			else if (m > 1) next = 1;
			else next = 0;
			{
				time_t until = next > 0 ? at - next * 60 : at;
				while (time(NULL) < until && (time(NULL) < at))
					sleep(1);
			}
		}
	}
	announce(at, 0);
	if (kill(1, sig_to_init) == -1) {
		fprintf(stderr, "shutdown: cannot signal init: %s\n", strerror(errno));
		unlink(PATH_NOLOGIN);
		return 1;
	}
	(void)what;
	return 0;
}

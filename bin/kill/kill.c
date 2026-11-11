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

/* kill - send a signal to processes, or list the signals (POSIX.1-2024).
 *
 *   kill -s signal_name pid ...
 *   kill -l [exit_status]
 *   kill [-signal_name] pid ...
 *   kill [-signal_number] pid ...
 *
 * The signal is named without SIG (TERM, HUP ...) or numbered; 0 only checks
 * that the process is there.  A negative pid is a process group.  The
 * default is TERM.  -l lists the names; with an exit status, the name of the
 * signal that ended the process (status 128+n, or n itself).  Job
 * specifications (%1) are the shell's.  The status is 1 if anything failed. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAXSIG 64

static void usage(void)
{
	fprintf(stderr, "usage: kill [-s signal_name] pid ...\n"
	    "       kill -l [exit_status]\n"
	    "       kill [-signal_name] pid ...\n"
	    "       kill [-signal_number] pid ...\n");
	exit(2);
}

/* a signal number (0 too) or a name; -1 if it is neither */
static int signal_of(const char *s)
{
	int sig;
	char *end;
	long v;

	if (*s >= '0' && *s <= '9') {
		errno = 0;
		v = strtol(s, &end, 10);
		if (*end || errno || v > MAXSIG)
			return -1;
		return (int)v;
	}
	if (str2sig(s, &sig) == 0)
		return sig;
	return -1;
}

static int list(const char *arg)
{
	char name[32];
	int i, n = 0;

	if (arg) {
		char *end;
		long v;
		errno = 0;
		v = strtol(arg, &end, 10);
		if (!*arg || *end || errno || v < 0 || v > 255) {
			fprintf(stderr, "kill: %s: not an exit status or signal number\n", arg);
			return 1;
		}
		if (v > 128)
			v -= 128; /* a status that says "killed by signal" */
		if (sig2str((int)v, name) != 0) {
			fprintf(stderr, "kill: %s: not a signal\n", arg);
			return 1;
		}
		puts(name);
		return 0;
	}
	for (i = 1; i <= MAXSIG; i++)
		if (sig2str(i, name) == 0)
			printf("%s%s", n++ ? " " : "", name);
	putchar('\n');
	return 0;
}

int main(int argc, char *argv[])
{
	int sig = SIGTERM, i, status = 0;

	if (argc < 2)
		usage();
	i = 1;
	if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "-L")) {
		if (argc > 3)
			usage();
		return list(argc == 3 ? argv[2] : NULL);
	}
	if (!strcmp(argv[i], "-s")) {
		if (argc < 4)
			usage();
		if ((sig = signal_of(argv[i + 1])) < 0) {
			fprintf(stderr, "kill: %s: unknown signal\n", argv[i + 1]);
			return 2;
		}
		i += 2;
	} else if (argv[i][0] == '-' && argv[i][1] && strcmp(argv[i], "--")) {
		if ((sig = signal_of(argv[i] + 1)) < 0) {
			fprintf(stderr, "kill: %s: unknown signal\n", argv[i] + 1);
			return 2;
		}
		i++;
	}
	if (i < argc && !strcmp(argv[i], "--"))
		i++;
	if (i >= argc)
		usage();
	for (; i < argc; i++) {
		char *end;
		long pid;
		errno = 0;
		pid = strtol(argv[i], &end, 10);
		if (!*argv[i] || *end || errno || pid > INT_MAX || pid < INT_MIN + 1) {
			fprintf(stderr, "kill: %s: not a process id\n", argv[i]);
			status = 1;
			continue;
		}
		if (kill((pid_t)pid, sig) == -1) {
			fprintf(stderr, "kill: %s: %s\n", argv[i], strerror(errno));
			status = 1;
		}
	}
	return status;
}

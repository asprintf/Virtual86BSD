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

/* getty - set up a terminal line and ask for a login name (the getty(8) of
   the BSDs, much reduced).
 *
 *   getty [-f gettytab] [type [line]]
 *
 * init(8) starts one for every line of /etc/ttys that is on: `getty std.115200
 * tty00'.  With a line, getty takes /dev/line from whoever has it
 * (revoke(2): their open files go dead and their session is hung up), makes
 * itself the leader of a new session with that terminal as its controlling
 * terminal and sets the owner to root, the mode 0600 (login gives the user
 * theirs).  With no line, or `-', the terminal is the standard input.  The
 * type names an entry of /etc/gettytab (default `default'); what an entry
 * lacks comes from its tc= entry and then from `default'.
 *
 * Then, for ever: set the terminal (speed sp, 8 bits, canonical input with
 * echo and erase/kill editing), print the banner (im: %h host name, %t line,
 * %s system, %m machine, %r release, %d date, %% a percent) and the prompt
 * (lm, `login: '), read a line, and run login -- name.  An empty line asks
 * again; so does a name that starts with `-', has a control character or is
 * longer than 32 bytes.  End of file ends getty with status 0 (init starts a
 * new one), a timeout (to, seconds) with status 1.  gettytab capabilities
 * used: sp, im, lm, lo (the login program), to; others are ignored.
 * What is not there: autobaud, port selectors, parity, upper-case-only
 * terminals, `nx' chains. */
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define PATH_GETTYTAB "/etc/gettytab"
#define PATH_LOGIN "/usr/bin/login"
#define NAME_MAX_LEN 32
#define DEFAULT_PROMPT "login: "

extern char **environ;

static const char *tabfile = PATH_GETTYTAB;
static char *tabdata;
static char line_name[64]; /* the line, without /dev/ */
static char hostname[256];
static struct utsname un;

static void die(const char *what, const char *why)
{
	fprintf(stderr, "getty: %s: %s\n", what, why);
	exit(1);
}

/* ---- gettytab ---- */

static void load_tab(void)
{
	FILE *f = fopen(tabfile, "r");
	size_t n = 0, cap = 4096;
	if (!f) {
		tabdata = strdup(""); /* no table: the built-in defaults */
		return;
	}
	tabdata = malloc(cap);
	if (!tabdata)
		die(tabfile, "out of memory");
	while (!feof(f)) {
		size_t r;
		if (n + 1024 > cap) {
			cap *= 2;
			if (!(tabdata = realloc(tabdata, cap)))
				die(tabfile, "out of memory");
		}
		r = fread(tabdata + n, 1, 1024, f);
		n += r;
		if (r == 0)
			break;
	}
	tabdata[n] = 0;
	fclose(f);
}

/* The text of the entry that has the name: its lines joined (a backslash at
   the end of a line continues it), comments left out; malloc'ed, or NULL. */
static char *find_entry(const char *name)
{
	const char *p = tabdata;
	size_t nl = strlen(name);

	while (*p) {
		const char *eol, *q;
		char *e;
		size_t len = 0;
		size_t cap;
		/* a logical line: from p to the first newline not after a backslash */
		eol = p;
		while (*eol && !(*eol == '\n' && !(eol > p && eol[-1] == '\\')))
			eol++;
		if (*p == '#' || *p == '\n' || *p == ' ' || *p == '\t') {
			p = *eol ? eol + 1 : eol;
			continue;
		}
		cap = (size_t)(eol - p) + 1;
		e = malloc(cap);
		if (!e)
			die(tabfile, "out of memory");
		for (q = p; q < eol; q++) {
			if (*q == '\\' && q[1] == '\n') {
				q++;
				while (q[1] == ' ' || q[1] == '\t')
					q++;
				continue;
			}
			e[len++] = *q;
		}
		e[len] = 0;
		/* names: up to the first colon, separated by | */
		{
			char *colon = strchr(e, ':'), *w, *save = NULL;
			char names[256];
			size_t k = colon ? (size_t)(colon - e) : len;
			if (k >= sizeof names)
				k = sizeof names - 1;
			memcpy(names, e, k);
			names[k] = 0;
			for (w = strtok_r(names, "|", &save); w; w = strtok_r(NULL, "|", &save))
				if (strlen(w) == nl && !strncmp(w, name, nl))
					return e;
		}
		free(e);
		p = *eol ? eol + 1 : eol;
	}
	return NULL;
}

/* the capability `cap' in the entry text: its value (a string with the
   escapes undone, or NULL for a boolean/number-less hit as ""), found = 1
   if there. */
static int entry_cap(const char *e, const char *cap, char *val, size_t vsize)
{
	size_t cl = strlen(cap);
	const char *p = strchr(e, ':');
	while (p && *p) {
		const char *s = p + 1, *q = s;
		/* a field ends at a colon not escaped */
		while (*q && *q != ':') {
			if (*q == '\\' && q[1])
				q++;
			q++;
		}
		if ((size_t)(q - s) >= cl && !strncmp(s, cap, cl) &&
		    (s[cl] == '=' || s[cl] == '#' || s[cl] == '@' || s + cl == q)) {
			if (val) {
				const char *v = s + cl;
				size_t o = 0;
				if (*v == '=' || *v == '#')
					v++;
				else if (*v == '@') { /* cancelled */
					val[0] = 0;
					return 2;
				}
				while (v < q && o + 1 < vsize) {
					if (*v == '\\' && v + 1 < q) {
						v++;
						switch (*v) {
						case 'e': case 'E': val[o++] = 033; v++; break;
						case 'r': val[o++] = '\r'; v++; break;
						case 'n': val[o++] = '\n'; v++; break;
						case 't': val[o++] = '\t'; v++; break;
						case 'b': val[o++] = '\b'; v++; break;
						case 'f': val[o++] = '\f'; v++; break;
						case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
							int n = 0, d = 0;
							while (d < 3 && v < q && *v >= '0' && *v <= '7') {
								n = n * 8 + (*v - '0');
								v++; d++;
							}
							val[o++] = (char)n;
							break;
						}
						default: val[o++] = *v++; break;
						}
					} else if (*v == '^' && v + 1 < q) {
						val[o++] = (char)(v[1] & 037);
						v += 2;
					} else {
						val[o++] = *v++;
					}
				}
				val[o] = 0;
			}
			return 1;
		}
		p = q;
	}
	return 0;
}

/* a capability from the entry `name', following its tc= entries, then from
   `default'; 1 if found (cancelled with @ counts as not found) */
static int get_cap(const char *name, const char *cap, char *val, size_t vsize)
{
	char cur[128], next[128];
	int depth, r;

	snprintf(cur, sizeof cur, "%s", name);
	for (depth = 0; depth < 8; depth++) {
		char *e = find_entry(cur);
		if (!e)
			break;
		r = entry_cap(e, cap, val, vsize);
		if (r) {
			free(e);
			return r == 1;
		}
		if (entry_cap(e, "tc", next, sizeof next) != 1) {
			free(e);
			break;
		}
		free(e);
		snprintf(cur, sizeof cur, "%s", next);
	}
	if (strcmp(name, "default")) {
		char *e = find_entry("default");
		if (e) {
			r = entry_cap(e, cap, val, vsize);
			free(e);
			return r == 1;
		}
	}
	return 0;
}

/* ---- the terminal ---- */

static speed_t baud(long n)
{
	switch (n) {
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
#ifdef B57600
	case 57600: return B57600;
#endif
#ifdef B115200
	case 115200: return B115200;
#endif
	case 4800: return B4800;
	case 2400: return B2400;
	case 1200: return B1200;
	case 600: return B600;
	case 300: return B300;
	}
	return B9600;
}

static void set_terminal(const char *type)
{
	struct termios t;
	char v[64];
	long sp = 9600;

	if (tcgetattr(0, &t) == -1)
		die(line_name[0] ? line_name : "stdin", strerror(errno));
	if (get_cap(type, "sp", v, sizeof v))
		sp = strtol(v, NULL, 10);
	t.c_iflag = BRKINT | ICRNL | IXON;
	t.c_oflag = OPOST | ONLCR;
	t.c_cflag = (t.c_cflag & ~(tcflag_t)(CSIZE | PARENB | PARODD | CSTOPB)) | CS8 | CREAD | HUPCL;
	t.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
#ifdef ECHOCTL
	t.c_lflag |= ECHOCTL;
#endif
#ifdef IEXTEN
	t.c_lflag |= IEXTEN;
#endif
	t.c_cc[VINTR] = 003;
	t.c_cc[VQUIT] = 034;
	t.c_cc[VERASE] = 0177;
	t.c_cc[VKILL] = 025;
	t.c_cc[VEOF] = 004;
	t.c_cc[VSUSP] = 032;
	t.c_cc[VSTART] = 021;
	t.c_cc[VSTOP] = 023;
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	cfsetispeed(&t, baud(sp));
	cfsetospeed(&t, baud(sp));
	tcflush(0, TCIFLUSH); /* only what was typed: not the message just written */
	if (tcsetattr(0, TCSANOW, &t) == -1)
		die(line_name[0] ? line_name : "stdin", strerror(errno));
}

static void add(char *out, size_t *o, size_t size, const char *s)
{
	size_t l = strlen(s);
	if (l > size - 1 - *o)
		l = size - 1 - *o;
	memcpy(out + *o, s, l);
	*o += l;
}

/* write a gettytab string with the % escapes of the banner */
static void putf(const char *s)
{
	char out[1024], one[2] = { 0, 0 }, date[64];
	size_t o = 0;
	for (; *s; s++) {
		if (*s != '%') {
			one[0] = *s;
			add(out, &o, sizeof out, one);
			continue;
		}
		switch (*++s) {
		case 'h': add(out, &o, sizeof out, hostname); break;
		case 't': add(out, &o, sizeof out, line_name); break;
		case 's': add(out, &o, sizeof out, un.sysname); break;
		case 'm': add(out, &o, sizeof out, un.machine); break;
		case 'r': add(out, &o, sizeof out, un.release); break;
		case 'd': {
			time_t now = time(NULL);
			struct tm *tm = localtime(&now);
			if (tm && strftime(date, sizeof date, "%a %b %e %H:%M:%S %Y", tm))
				add(out, &o, sizeof out, date);
			break;
		}
		case '%': add(out, &o, sizeof out, "%"); break;
		case 0: s--; break;
		default:
			add(out, &o, sizeof out, "%");
			one[0] = *s;
			add(out, &o, sizeof out, one);
			break;
		}
	}
	write(1, out, o);
}

static void on_alarm(int sig)
{
	(void)sig;
	_exit(1);
}

static const char *reject; /* why the last name was not taken, or NULL */

/* the login name from the terminal (canonical input: the line is edited by
   the terminal); NULL at end of file; with reject set, the name is not good */
static char *get_name(char *name, size_t size)
{
	ssize_t n;
	size_t i;
	for (;;) {
		n = read(0, name, size - 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return NULL;
		break;
	}
	if (name[n - 1] == '\n') {
		name[n - 1] = 0;
		if (n > 1 && name[n - 2] == '\r')
			name[n - 2] = 0;
	} else if ((size_t)n == size - 1) {
		/* the buffer is full and no newline: too long; drop the rest of the line */
		char c;
		while (read(0, &c, 1) == 1 && c != '\n')
			;
		reject = "login name too long.\r\n";
		name[0] = 0;
		return name;
	} else {
		name[n] = 0; /* a line ended by the EOF character after text */
	}
	for (i = 0; name[i]; i++) {
		if ((unsigned char)name[i] < 040 || name[i] == 0177) {
			reject = "login name not accepted.\r\n";
			name[0] = 0;
			return name;
		}
	}
	return name;
}

int main(int argc, char *argv[])
{
	const char *type = "default", *line = NULL;
	char name[128], prompt[64], banner[512], login[PATH_MAX], tmo[16];
	int ch;

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while ((ch = getopt(argc, argv, "f:")) != -1) {
		if (ch != 'f') {
			fprintf(stderr, "usage: getty [-f gettytab] [type [line]]\n");
			return 1;
		}
		tabfile = optarg;
	}
	argc -= optind;
	argv += optind;
	if (argc > 2) {
		fprintf(stderr, "usage: getty [-f gettytab] [type [line]]\n");
		return 1;
	}
	if (argc >= 1)
		type = argv[0];
	if (argc == 2 && strcmp(argv[1], "-"))
		line = argv[1];

	gethostname(hostname, sizeof hostname);
	hostname[sizeof hostname - 1] = 0;
	if (!hostname[0])
		strcpy(hostname, "Amnesiac");
	uname(&un);
	load_tab();

	if (line) {
		char dev[96];
		int fd;
		if (strchr(line, '/') || line[0] == '.' || strlen(line) >= sizeof line_name)
			die(line, "not a terminal line name");
		snprintf(dev, sizeof dev, "/dev/%s", line);
		snprintf(line_name, sizeof line_name, "%s", line);
		chown(dev, 0, 0);
		chmod(dev, 0600);
		revoke(dev);
		setsid(); /* (init has made us a session leader already: then this fails, harmlessly) */
		fd = open(dev, O_RDWR);
		if (fd < 0)
			die(dev, strerror(errno));
#ifdef TIOCSCTTY
		ioctl(fd, TIOCSCTTY, 0); /* a no-op if open made it ours */
#endif
		if (fd != 0)
			dup2(fd, 0);
		dup2(0, 1);
		dup2(0, 2);
		if (fd > 2)
			close(fd);
		tcsetpgrp(0, getpid());
	} else {
		const char *tn = ttyname(0);
		if (!tn)
			die("stdin", "not a terminal");
		snprintf(line_name, sizeof line_name, "%s", strncmp(tn, "/dev/", 5) ? tn : tn + 5);
	}

	if (!get_cap(type, "lm", prompt, sizeof prompt))
		strcpy(prompt, DEFAULT_PROMPT);
	if (!get_cap(type, "im", banner, sizeof banner))
		snprintf(banner, sizeof banner, "\r\n%%s/%%m (%%h) (%%t)\r\n\r\n");
	if (!get_cap(type, "lo", login, sizeof login))
		strcpy(login, PATH_LOGIN);

	for (;;) {
		char *n;
		long to = 0;
		set_terminal(type);
		putf(banner);
		write(1, prompt, strlen(prompt));
		if (get_cap(type, "to", tmo, sizeof tmo))
			to = strtol(tmo, NULL, 10);
		if (to > 0) {
			signal(SIGALRM, on_alarm);
			alarm((unsigned)to);
		}
		n = get_name(name, sizeof name);
		alarm(0);
		if (!n)
			return 0; /* end of file: init starts a new getty */
		if (reject) {
			write(1, reject, strlen(reject));
			reject = NULL;
			continue;
		}
		if (n[0] == 0)
			continue;
		if (n[0] == '-') {
			static const char m[] = "login names may not start with '-'.\r\n";
			write(1, m, sizeof m - 1);
			continue;
		}
		if (strlen(n) > NAME_MAX_LEN) {
			static const char m[] = "login name too long.\r\n";
			write(1, m, sizeof m - 1);
			continue;
		}
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGALRM, SIG_DFL);
		execle(login, "login", "--", n, (char *)NULL, environ);
		die(login, strerror(errno));
	}
}

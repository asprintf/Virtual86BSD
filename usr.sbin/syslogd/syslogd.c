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

/* syslogd - log system messages.
 *
 *   syslogd [-d] [-f config] [-k klog] [-m minutes] [-P pidfile] [-p socket]
 *
 * Messages come from two places: the programs that call syslog(3) or
 * logger(1), which send RFC 3164 lines ("<PRI>Mmm dd hh:mm:ss TAG[PID]:
 * text") to the datagram socket /dev/log, and the kernel, whose messages
 * are read from /dev/klog (each one is "/bsd: text", with the priority
 * LOG_KERN|LOG_CRIT unless the line starts with "<N>").  The time stamp is
 * always the daemon's own: a program cannot back-date a line.
 *
 * Each message is matched against /etc/syslog.conf (see syslog.conf(5) of
 * OpenBSD; the part that is here):
 *
 *   selector[;selector...]   action
 *
 * A selector is facility[,facility...].level; a facility is a name of
 * <syslog.h> (kern user mail daemon auth syslog lpr news uucp cron
 * authpriv ftp local0-7, security for auth, mark) or `*' for all of them.
 * The level selects that level and every one more severe (emerg alert crit
 * err warning notice info debug); =level only that one; !level and !=level
 * take them away from what the selectors before it chose; `none' takes the
 * facility away.  `*' as a level is debug.  An action is a file (a
 * path), `*' (every user logged in) or a comma-separated list of users
 * (their terminals).  Forwarding (@host), pipes (|cmd), program and host
 * blocks are not supported: they are reported and the line is ignored.
 * Without a configuration file: *.err to /dev/console, *.emerg to every
 * user.
 *
 * A file is created (mode 0640) if it is not there, and appended to.  A
 * line that repeats the one before it in a file is not written again: a
 * "last message repeated N times" line replaces the copies when something
 * else comes, or after 30 seconds.  Control characters of a message are
 * shown as ^X, so that a message cannot move the cursor or fake a line.
 * Files are synced to disk a second after the messages stop (at most 5
 * seconds when they do not).  An entry `mark' in a selector gets "-- MARK
 * --" every -m minutes (default 20, 0 for never).
 *
 * SIGHUP rereads the configuration and opens the files again (for log
 * rotation); SIGTERM, SIGINT and SIGQUIT log it and stop.  The process id
 * is written to /var/run/syslog.pid.  -d stays in the foreground and
 * shows the lines on the standard error.  -p, -k, -f and -P name another
 * socket, kernel log device, configuration file and pid file. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#define PATH_LOGSOCK "/dev/log"
#define PATH_KLOG    "/dev/klog"
#define PATH_CONF    "/etc/syslog.conf"
#define PATH_PID     "/var/run/syslog.pid"
#define PATH_UNIX    "/bsd"

#define MAXLINE   1024 /* the longest message kept (what is more is cut) */
#define NFAC      LOG_NFACILITIES
#define FAC_MARK  NFAC /* the internal facility of the mark messages */
#define DEFUPRI   (LOG_USER | LOG_NOTICE)
#define DEFSPRI   (LOG_KERN | LOG_CRIT)
#define REPEAT_S  30   /* seconds after which repeated lines are told */

enum { F_FILE, F_USERS, F_WALL };

struct filed {
	struct filed *next;
	int type;
	uint8_t pmask[NFAC + 1]; /* the levels (bit n: level n) chosen per facility */
	char *name;              /* the path, or the users */
	int fd;                  /* the file, or -1 (opened at the next message) */
	int tty;                 /* a terminal: lines end in CR LF */
	int dirty;               /* written since the last fsync */
	char prev[MAXLINE + 300]; /* "host text" of the last line written */
	int repeats;             /* times the line was not written again */
	time_t repeat_since;     /* monotonic seconds when the first copy came */
};

static struct filed *files;
static const char *conffile = PATH_CONF, *klogpath = PATH_KLOG, *sockpath = PATH_LOGSOCK, *pidpath = PATH_PID;
static int debug, markmin = 20;
static int sockfd = -1, klogfd = -1;
static int selfpipe[2] = {-1, -1};
static volatile sig_atomic_t want_exit, want_hup;
static int exit_sig;
static time_t dirty_since, next_mark;
static int bound; /* the socket is ours: remove it at the end */

static const struct { const char *name; int val; } facnames[] = {
	{"kern", 0}, {"user", 1}, {"mail", 2}, {"daemon", 3}, {"auth", 4}, {"security", 4},
	{"syslog", 5}, {"lpr", 6}, {"news", 7}, {"uucp", 8}, {"cron", 9}, {"authpriv", 10},
	{"ftp", 11}, {"local0", 16}, {"local1", 17}, {"local2", 18}, {"local3", 19},
	{"local4", 20}, {"local5", 21}, {"local6", 22}, {"local7", 23}, {"mark", FAC_MARK},
	{NULL, 0}
};
static const struct { const char *name; int val; } prinames[] = {
	{"emerg", 0}, {"panic", 0}, {"alert", 1}, {"crit", 2}, {"err", 3}, {"error", 3},
	{"warning", 4}, {"warn", 4}, {"notice", 5}, {"info", 6}, {"debug", 7}, {NULL, 0}
};

static time_t monotonic(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		return ts.tv_sec;
	return time(NULL);
}

static void usage(void)
{
	fprintf(stderr, "usage: syslogd [-d] [-f config_file] [-k klog_device] [-m mark_interval]\n"
	    "               [-P pid_file] [-p log_socket]\n");
	exit(1);
}

/* ---- the text of a message ---- */

/* control characters as ^X (DEL as ^?): a message cannot act as the
   terminal's commands or break the line; 8-bit bytes pass (UTF-8) */
static size_t vis(char *dst, size_t dsz, const char *src, size_t n)
{
	size_t i, j = 0;
	for (i = 0; i < n; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c < 0x20 || c == 0x7f) {
			if (j + 2 >= dsz)
				break;
			dst[j++] = '^';
			dst[j++] = c == 0x7f ? '?' : (char)(c + '@');
		} else {
			if (j + 1 >= dsz)
				break;
			dst[j++] = (char)c;
		}
	}
	dst[j] = 0;
	return j;
}

static const char monname[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* "Mmm dd hh:mm:ss" of now (the day space padded) */
static void stamp(char *buf, size_t size, time_t t)
{
	struct tm tm;
	if (!localtime_r(&t, &tm)) {
		snprintf(buf, size, "Jan  1 00:00:00");
		return;
	}
	snprintf(buf, size, "%s %2d %02d:%02d:%02d", monname[tm.tm_mon % 12], tm.tm_mday, tm.tm_hour,
	    tm.tm_min, tm.tm_sec);
}

/* a well-formed "Mmm dd hh:mm:ss " at the start of s */
static int has_stamp(const char *s, size_t n)
{
	int i;
	if (n < 16 || s[3] != ' ' || s[6] != ' ' || s[9] != ':' || s[12] != ':' || s[15] != ' ')
		return 0;
	for (i = 0; i < 12; i++)
		if (monname[i][0] == s[0] && monname[i][1] == s[1] && monname[i][2] == s[2])
			break;
	if (i == 12)
		return 0;
	return (s[4] == ' ' || (s[4] >= '0' && s[4] <= '3')) && s[5] >= '0' && s[5] <= '9' &&
	    s[7] >= '0' && s[7] <= '2' && s[8] >= '0' && s[8] <= '9' && s[10] >= '0' && s[10] <= '5' &&
	    s[11] >= '0' && s[11] <= '9' && s[13] >= '0' && s[13] <= '5' && s[14] >= '0' && s[14] <= '9';
}

/* ---- the files ---- */

static void closefile(struct filed *f)
{
	if (f->fd >= 0) {
		if (f->dirty)
			fsync(f->fd);
		close(f->fd);
	}
	f->fd = -1;
	f->dirty = 0;
}

static void openfile(struct filed *f)
{
	struct stat st;
	f->fd = open(f->name, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY | O_NONBLOCK | O_CLOEXEC, 0640);
	if (f->fd < 0)
		return;
	f->tty = fstat(f->fd, &st) == 0 && S_ISCHR(st.st_mode) && isatty(f->fd);
}

static void sync_all(void)
{
	struct filed *f;
	for (f = files; f; f = f->next)
		if (f->fd >= 0 && f->dirty) {
			fsync(f->fd);
			f->dirty = 0;
		}
	dirty_since = 0;
}

/* write one line (without its end) to a file entry */
static void writeline(struct filed *f, const char *line)
{
	char buf[MAXLINE + 400];
	size_t n;
	ssize_t w;

	if (f->fd < 0)
		openfile(f);
	if (f->fd < 0)
		return;
	n = (size_t)snprintf(buf, sizeof buf, "%s%s", line, f->tty ? "\r\n" : "\n");
	if (n >= sizeof buf)
		n = sizeof buf - 1;
	do
		w = write(f->fd, buf, n);
	while (w < 0 && errno == EINTR);
	if (w < 0) {
		if (errno != EAGAIN) {
			/* a file that went away or a full disk: start again at the next message */
			f->dirty = 0;
			close(f->fd);
			f->fd = -1;
		}
		return;
	}
	if (!f->tty) {
		f->dirty = 1;
		if (!dirty_since)
			dirty_since = monotonic();
	}
}

/* tell the users logged in (the terminals of utmpx); users NULL: all of them */
static void wallusers(const char *users, const char *text)
{
	struct utmpx *u;
	char dev[sizeof u->ut_line + 8], name[sizeof u->ut_user + 1], msg[MAXLINE + 400];
	int n;

	n = snprintf(msg, sizeof msg, "\r\n\a%s\r\n", text);
	if (n >= (int)sizeof msg)
		n = (int)sizeof msg - 1;
	setutxent();
	while ((u = getutxent())) {
		int fd, ok = users == NULL;
		size_t l;
		if (u->ut_type != USER_PROCESS || !u->ut_line[0])
			continue;
		l = strnlen(u->ut_user, sizeof u->ut_user);
		memcpy(name, u->ut_user, l);
		name[l] = 0;
		if (users) {
			const char *p = users;
			while (*p && !ok) {
				size_t k = strcspn(p, ",");
				if (k == l && !strncmp(p, name, k))
					ok = 1;
				p += k + (p[k] == ',');
			}
		}
		if (!ok)
			continue;
		l = strnlen(u->ut_line, sizeof u->ut_line);
		if (memchr(u->ut_line, '/', l) && strncmp(u->ut_line, "pts/", 4))
			continue;
		snprintf(dev, sizeof dev, "/dev/%.*s", (int)l, u->ut_line);
		fd = open(dev, O_WRONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
		if (fd < 0)
			continue;
		if (isatty(fd))
			(void)!write(fd, msg, (size_t)n);
		close(fd);
	}
	endutxent();
}

/* the line of a file entry that the repeats stand for */
static void flush_repeats(struct filed *f, time_t now)
{
	char ts[20], line[MAXLINE + 300], host[256], text[100];
	size_t k;

	if (f->repeats <= 0)
		return;
	k = strcspn(f->prev, " ");
	snprintf(host, sizeof host, "%.*s", (int)(k > 255 ? 255 : k), f->prev);
	stamp(ts, sizeof ts, now);
	snprintf(text, sizeof text, "last message repeated %d time%s", f->repeats, f->repeats == 1 ? "" : "s");
	snprintf(line, sizeof line, "%s %s %s", ts, host, text);
	if (f->type == F_FILE)
		writeline(f, line);
	f->repeats = 0;
}

/* ---- logging a message ---- */

/* the host name, without its domain */
static void myhost(char *buf, size_t size)
{
	char *dot;
	if (gethostname(buf, size) < 0)
		snprintf(buf, size, "localhost");
	buf[size - 1] = 0;
	if ((dot = strchr(buf, '.')))
		*dot = 0;
	if (!*buf)
		snprintf(buf, size, "localhost");
}

/* pri: facility and level (FAC_MARK << 3 for a mark); text: "tag[pid]: message" */
static void logmsg(int pri, const char *text)
{
	struct filed *f;
	char host[256], ts[20], key[MAXLINE + 300], line[MAXLINE + 400];
	int fac = (pri & LOG_FACMASK) >> 3, lvl = pri & LOG_PRIMASK, mark = fac == FAC_MARK;
	time_t now = time(NULL), mono = monotonic();

	if (fac > FAC_MARK)
		fac = LOG_FAC(DEFUPRI);
	myhost(host, sizeof host);
	stamp(ts, sizeof ts, now);
	snprintf(key, sizeof key, "%s %s", host, text);
	snprintf(line, sizeof line, "%s %s", ts, key);
	if (debug)
		fprintf(stderr, "%s\n", line);
	for (f = files; f; f = f->next) {
		if (!(f->pmask[fac] & (1 << lvl)))
			continue;
		switch (f->type) {
		case F_FILE:
			if (!mark && !strcmp(f->prev, key)) {
				if (f->repeats++ == 0)
					f->repeat_since = mono;
				continue;
			}
			flush_repeats(f, now);
			writeline(f, line);
			snprintf(f->prev, sizeof f->prev, "%s", mark ? "" : key);
			break;
		case F_USERS:
			wallusers(f->name, line);
			break;
		case F_WALL:
			wallusers(NULL, line);
			break;
		}
	}
}

/* a message of the daemon's own */
static void logsys(int pri, const char *fmt, ...)
{
	char text[MAXLINE + 16], msg[MAXLINE];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	snprintf(text, sizeof text, "syslogd: %s", msg);
	logmsg(LOG_SYSLOG | pri, text);
}

/* the "<N>" priority at the start of s, and the rest of the line */
static int takepri(const char **sp, size_t *np, int def)
{
	const char *s = *sp;
	size_t n = *np;
	int pri = def;

	if (n >= 3 && s[0] == '<') {
		size_t i = 1;
		int v = 0;
		while (i < n && s[i] >= '0' && s[i] <= '9' && v < 10000)
			v = v * 10 + (s[i++] - '0');
		if (i < n && s[i] == '>' && i > 1) {
			/* a priority of a real facility; else the text is the message */
			if ((v >> 3) < NFAC) {
				pri = v;
				*sp = s + i + 1;
				*np = n - i - 1;
			}
		}
	}
	return pri;
}

/* a datagram of /dev/log */
static void printline(const char *s, size_t n)
{
	char text[MAXLINE + 1];
	int pri;

	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\0' || s[n - 1] == '\r'))
		n--;
	if (n == 0)
		return;
	pri = takepri(&s, &n, DEFUPRI);
	if (has_stamp(s, n)) {
		s += 16;
		n -= 16;
	}
	if (n > MAXLINE)
		n = MAXLINE;
	vis(text, sizeof text, s, n);
	logmsg(pri, text);
}

/* a line of the kernel */
static void printsys(const char *s, size_t n)
{
	char text[MAXLINE + 16];
	char body[MAXLINE + 1];
	int pri;

	while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n'))
		n--;
	if (n == 0)
		return;
	pri = takepri(&s, &n, DEFSPRI);
	if (n > MAXLINE - 8)
		n = MAXLINE - 8;
	vis(body, sizeof body, s, n);
	snprintf(text, sizeof text, "%s: %s", PATH_UNIX, body);
	logmsg(pri, text);
}

/* ---- the configuration ---- */

static void free_files(void)
{
	struct filed *f, *next;
	for (f = files; f; f = next) {
		next = f->next;
		flush_repeats(f, time(NULL));
		closefile(f);
		free(f->name);
		free(f);
	}
	files = NULL;
}

/* the text up to the next delimiter, ended; *sp goes past it (NULL at the end) */
static char *split(char **sp, const char *delims)
{
	char *s = *sp, *e;
	if (!s)
		return NULL;
	e = s + strcspn(s, delims);
	if (*e) {
		*e = 0;
		*sp = e + 1;
	} else
		*sp = NULL;
	return s;
}

static int facility_of(const char *name)
{
	int i;
	for (i = 0; facnames[i].name; i++)
		if (!strcasecmp(name, facnames[i].name))
			return facnames[i].val;
	return -1;
}

static int level_of(const char *name)
{
	int i;
	if (!strcmp(name, "*"))
		return LOG_DEBUG;
	for (i = 0; prinames[i].name; i++)
		if (!strcasecmp(name, prinames[i].name))
			return prinames[i].val;
	return -1;
}

/* one selector, "facs.level", into a mask; an error message, or NULL */
static const char *selector(uint8_t *pmask, char *sel)
{
	static char err[100];
	char *dot = strchr(sel, '.'), *facs, *lvl, *name;
	int neg = 0, eq = 0, none = 0, level = 0, i;

	if (!dot || dot == sel || !dot[1])
		return "bad selector";
	*dot = 0;
	facs = sel;
	lvl = dot + 1;
	while (*lvl == '!' || *lvl == '=') {
		if (*lvl == '!')
			neg = 1;
		else
			eq = 1;
		lvl++;
	}
	if (!strcasecmp(lvl, "none") && !neg && !eq)
		none = 1;
	else if ((level = level_of(lvl)) < 0) {
		snprintf(err, sizeof err, "unknown priority name \"%.40s\"", lvl);
		return err;
	}
	while ((name = split(&facs, ","))) {
		int bits = eq ? 1 << level : (1 << (level + 1)) - 1, lo, hi;
		if (!strcmp(name, "*")) {
			lo = 0;
			hi = NFAC - 1;
		} else if ((lo = facility_of(name)) < 0) {
			snprintf(err, sizeof err, "unknown facility name \"%.40s\"", name);
			return err;
		} else
			hi = lo;
		for (i = lo; i <= hi; i++) {
			if (none)
				pmask[i] = 0;
			else if (neg)
				pmask[i] &= (uint8_t)~bits;
			else
				pmask[i] |= (uint8_t)bits;
		}
	}
	return NULL;
}

/* one line of the configuration file: a new entry on the list, or a complaint */
static void cfline(char *line, int lineno)
{
	struct filed *f, **pp;
	char *act, *sel, *p;
	const char *err = NULL;
	size_t k;

	if (line[0] == '!' || line[0] == '+') {
		fprintf(stderr, "syslogd: %s: line %d: program and host blocks are not supported\n", conffile, lineno);
		return;
	}
	k = strcspn(line, " \t");
	if (!line[k]) {
		fprintf(stderr, "syslogd: %s: line %d: no action\n", conffile, lineno);
		return;
	}
	line[k] = 0;
	act = line + k + 1;
	act += strspn(act, " \t");
	if (!*act) {
		fprintf(stderr, "syslogd: %s: line %d: no action\n", conffile, lineno);
		return;
	}
	if (*act == '@' || *act == '|' || *act == '-') {
		fprintf(stderr, "syslogd: %s: line %d: forwarding, pipes and unsynced files are not supported\n",
		    conffile, lineno);
		return;
	}
	f = calloc(1, sizeof *f);
	if (!f)
		return;
	f->fd = -1;
	p = line;
	while ((sel = split(&p, ";")) && !err)
		err = selector(f->pmask, sel);
	if (err) {
		fprintf(stderr, "syslogd: %s: line %d: %s\n", conffile, lineno, err);
		free(f);
		return;
	}
	if (*act == '/') {
		f->type = F_FILE;
	} else if (!strcmp(act, "*")) {
		f->type = F_WALL;
	} else {
		f->type = F_USERS;
		for (p = act; *p; p++)
			if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
			    strchr("._-,", *p))) {
				fprintf(stderr, "syslogd: %s: line %d: not an action: \"%s\"\n", conffile, lineno, act);
				free(f);
				return;
			}
	}
	f->name = strdup(act);
	if (!f->name) {
		free(f);
		return;
	}
	for (pp = &files; *pp; pp = &(*pp)->next)
		;
	*pp = f;
}

static void default_config(void)
{
	char a[] = "*.err\t/dev/console", b[] = "*.emerg\t*";
	cfline(a, 0);
	cfline(b, 0);
}

static void init(void)
{
	FILE *fp;
	char buf[2048], *p;
	int lineno = 0;
	struct filed *f;

	free_files();
	fp = fopen(conffile, "r");
	if (!fp) {
		if (errno != ENOENT)
			fprintf(stderr, "syslogd: %s: %s\n", conffile, strerror(errno));
		default_config();
	} else {
		while (fgets(buf, sizeof buf, fp)) {
			size_t n = strlen(buf);
			lineno++;
			if (n == sizeof buf - 1 && buf[n - 1] != '\n') {
				int c;
				while ((c = getc(fp)) != EOF && c != '\n')
					;
				fprintf(stderr, "syslogd: %s: line %d: too long\n", conffile, lineno);
				continue;
			}
			while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\t'))
				buf[--n] = 0;
			p = buf + strspn(buf, " \t");
			if (!*p || *p == '#')
				continue;
			cfline(p, lineno);
		}
		fclose(fp);
	}
	for (f = files; f; f = f->next)
		if (f->type == F_FILE) {
			openfile(f);
			if (f->fd < 0)
				fprintf(stderr, "syslogd: %s: %s\n", f->name, strerror(errno));
		}
}

/* ---- sockets, signals, the loop ---- */

static void sighandler(int sig)
{
	int e = errno;
	if (sig == SIGHUP)
		want_hup = 1;
	else {
		want_exit = 1;
		exit_sig = sig;
	}
	if (selfpipe[1] >= 0)
		(void)!write(selfpipe[1], "x", 1);
	errno = e;
}

/* another daemon is running if the pid file names a live process */
static int running(void)
{
	FILE *fp = fopen(pidpath, "r");
	long pid;
	int r = 0;
	if (!fp)
		return 0;
	if (fscanf(fp, "%ld", &pid) == 1 && pid > 1 && pid != (long)getpid() &&
	    (kill((pid_t)pid, 0) == 0 || errno == EPERM))
		r = 1;
	fclose(fp);
	return r;
}

static int opensocket(void)
{
	struct sockaddr_un addr;
	int fd, fl;

	if (strlen(sockpath) >= sizeof addr.sun_path) {
		fprintf(stderr, "syslogd: %s: name too long\n", sockpath);
		return -1;
	}
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sockpath);
	fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		fprintf(stderr, "syslogd: socket: %s\n", strerror(errno));
		return -1;
	}
	unlink(sockpath);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		fprintf(stderr, "syslogd: %s: %s\n", sockpath, strerror(errno));
		close(fd);
		return -1;
	}
	bound = 1;
	chmod(sockpath, 0666); /* anyone may log */
	if ((fl = fcntl(fd, F_GETFL)) >= 0)
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	return fd;
}

static void writepid(pid_t pid)
{
	FILE *fp = fopen(pidpath, "w");
	if (!fp) {
		fprintf(stderr, "syslogd: %s: %s\n", pidpath, strerror(errno));
		return;
	}
	fprintf(fp, "%ld\n", (long)pid);
	fclose(fp);
	chmod(pidpath, 0644);
}

static void readklog(void)
{
	static char kbuf[2 * MAXLINE];
	static size_t klen;
	ssize_t n;
	char *nl;
	size_t used;

	n = read(klogfd, kbuf + klen, sizeof kbuf - klen);
	if (n <= 0) {
		if (n < 0 && (errno == EAGAIN || errno == EINTR))
			return;
		close(klogfd); /* the device is gone */
		klogfd = -1;
		return;
	}
	klen += (size_t)n;
	used = 0;
	while ((nl = memchr(kbuf + used, '\n', klen - used))) {
		printsys(kbuf + used, (size_t)(nl - (kbuf + used)));
		used = (size_t)(nl - kbuf) + 1;
	}
	if (used == 0 && klen == sizeof kbuf) { /* a line that does not end: cut it here */
		printsys(kbuf, klen);
		used = klen;
	}
	memmove(kbuf, kbuf + used, klen - used);
	klen -= used;
}

static void readsocket(void)
{
	char buf[MAXLINE * 2];
	int i;
	for (i = 0; i < 64; i++) {
		ssize_t n = recv(sockfd, buf, sizeof buf, 0);
		if (n < 0)
			break;
		printline(buf, (size_t)n);
	}
}

/* everything that waits for a time: repeats, the sync, the mark */
static void ticks(void)
{
	struct filed *f;
	time_t mono = monotonic(), now = time(NULL);

	for (f = files; f; f = f->next)
		if (f->repeats > 0 && mono - f->repeat_since >= REPEAT_S)
			flush_repeats(f, now);
	if (dirty_since && mono - dirty_since >= 1)
		sync_all();
	if (markmin > 0 && next_mark && mono >= next_mark) {
		logmsg((FAC_MARK << 3) | LOG_INFO, "-- MARK --");
		next_mark = mono + markmin * 60;
	}
}

static int timeout_ms(void)
{
	struct filed *f;
	time_t mono = monotonic(), t, best = -1;

	if (dirty_since) {
		t = dirty_since + 1 - mono;
		best = t < 0 ? 0 : t;
	}
	for (f = files; f; f = f->next)
		if (f->repeats > 0) {
			t = f->repeat_since + REPEAT_S - mono;
			if (t < 0)
				t = 0;
			if (best < 0 || t < best)
				best = t;
		}
	if (markmin > 0 && next_mark) {
		t = next_mark - mono;
		if (t < 0)
			t = 0;
		if (best < 0 || t < best)
			best = t;
	}
	return best < 0 ? -1 : (int)(best * 1000 + (dirty_since ? 100 : 0));
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	struct pollfd pfd[3];
	int ch;

	while ((ch = getopt(argc, argv, "df:k:m:P:p:")) != -1) {
		switch (ch) {
		case 'd': debug = 1; break;
		case 'f': conffile = optarg; break;
		case 'k': klogpath = optarg; break;
		case 'm': {
			char *end;
			long v = strtol(optarg, &end, 10);
			if (*optarg == 0 || *end || v < 0 || v > 7 * 24 * 60) {
				fprintf(stderr, "syslogd: bad mark interval: %s\n", optarg);
				return 1;
			}
			markmin = (int)v;
			break;
		}
		case 'P': pidpath = optarg; break;
		case 'p': sockpath = optarg; break;
		default: usage();
		}
	}
	if (optind != argc)
		usage();

	if (running()) {
		fprintf(stderr, "syslogd: already running\n");
		return 1;
	}
	umask(027);
	if ((sockfd = opensocket()) < 0)
		return 1;
	if ((klogfd = open(klogpath, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) < 0)
		fprintf(stderr, "syslogd: %s: %s\n", klogpath, strerror(errno));
	if (pipe(selfpipe) < 0) {
		fprintf(stderr, "syslogd: pipe: %s\n", strerror(errno));
		return 1;
	}
	for (ch = 0; ch < 2; ch++) {
		fcntl(selfpipe[ch], F_SETFD, FD_CLOEXEC);
		fcntl(selfpipe[ch], F_SETFL, fcntl(selfpipe[ch], F_GETFL) | O_NONBLOCK);
	}
	init();

	if (!debug) {
		pid_t pid = fork();
		if (pid < 0) {
			fprintf(stderr, "syslogd: fork: %s\n", strerror(errno));
			return 1;
		}
		if (pid > 0) {
			writepid(pid); /* (before it returns: whoever starts it may look at once) */
			_exit(0);
		}
		setsid();
		if (chdir("/") < 0)
			_exit(1);
		ch = open("/dev/null", O_RDWR);
		if (ch >= 0) {
			dup2(ch, 0);
			dup2(ch, 1);
			dup2(ch, 2);
			if (ch > 2)
				close(ch);
		}
	} else
		writepid(getpid());

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	if (markmin > 0)
		next_mark = monotonic() + markmin * 60;

	logsys(LOG_INFO, "start");

	for (;;) {
		int n, nfds = 0, ksel = -1;
		char drain[64];

		if (want_exit)
			break;
		if (want_hup) {
			want_hup = 0;
			logsys(LOG_INFO, "restart");
			init();
			logsys(LOG_INFO, "restarted");
		}
		pfd[nfds].fd = sockfd;
		pfd[nfds++].events = POLLIN;
		pfd[nfds].fd = selfpipe[0];
		pfd[nfds++].events = POLLIN;
		if (klogfd >= 0) {
			ksel = nfds;
			pfd[nfds].fd = klogfd;
			pfd[nfds++].events = POLLIN;
		}
		n = poll(pfd, (nfds_t)nfds, timeout_ms());
		if (n > 0) {
			if (pfd[1].revents & POLLIN)
				while (read(selfpipe[0], drain, sizeof drain) > 0)
					;
			if (pfd[0].revents & POLLIN)
				readsocket();
			if (ksel >= 0 && (pfd[ksel].revents & (POLLIN | POLLERR | POLLHUP)))
				readklog();
		}
		ticks();
	}
	logsys(LOG_INFO, "exiting on signal %d", exit_sig);
	free_files();
	if (bound)
		unlink(sockpath);
	unlink(pidpath);
	return 0;
}

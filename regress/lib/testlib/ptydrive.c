/* ptydrive - run a command on a pseudo-terminal and talk to it like a
 * person at a keyboard (for tests of getty, login, passwd ...).
 *
 *   ptydrive [-T seconds] [-o file] op ... -- command [argument ...]
 *
 * The command (looked up in PATH) is run in a new session with the pseudo-terminal as its
 * controlling terminal and its standard input, output and error.  Each op
 * is done in turn:
 *   e:TEXT   wait until the terminal has output TEXT (since the last e:)
 *   s:TEXT   type TEXT (escapes: \n \r \t \\ \e \^X control X \xHH)
 *   d:       type the end-of-file character (^D)
 *   t:MS     wait MS milliseconds
 *   w:       wait for the command to end (the rest of its output is read)
 *   n:TEXT   check that the terminal has NOT output TEXT since the last e:
 *            (after t: gives it time)
 * The transcript, the terminal's output with the carriage returns taken
 * out, is written to the standard output at the end, followed by a line
 * "[exit N]" or "[signal N]" if the command has ended, else "[running]"
 * (and it is killed).  Status: 0; 2 a usage error; 3 if an e: or w: timed
 * out (the transcript is written, with "[timeout]"); 4 if an n: found its
 * text.  -o: the terminal's output, as it came (with the carriage
 * returns), is also written to file. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char buf[1 << 16];
static size_t len, mark; /* the output so far; where the last e: ended */
static int master = -1, timeout_s = 20;
static const char *rawfile;
static pid_t child;
static int ended, status;

static void unescape(const char *s, char *out, size_t *n)
{
	size_t o = 0;
	for (; *s; s++) {
		if (*s == '\\' && s[1]) {
			s++;
			switch (*s) {
			case 'n': out[o++] = '\n'; break;
			case 'r': out[o++] = '\r'; break;
			case 't': out[o++] = '\t'; break;
			case 'e': out[o++] = 033; break;
			case '^': if (s[1]) { out[o++] = (char)(s[1] & 037); s++; } break;
			case 'x': {
				char h[3] = { s[1], s[1] ? s[2] : 0, 0 };
				out[o++] = (char)strtol(h, NULL, 16);
				s += s[1] ? (s[2] ? 2 : 1) : 0;
				break;
			}
			default: out[o++] = *s; break;
			}
		} else {
			out[o++] = *s;
		}
	}
	out[o] = 0;
	*n = o;
}

/* read what is there (waiting up to ms for the first byte); 0 at the end */
static int pump(int ms)
{
	struct pollfd p = { master, POLLIN, 0 };
	int r = poll(&p, 1, ms);
	if (r > 0 && (p.revents & (POLLIN | POLLHUP))) {
		ssize_t n = read(master, buf + len, sizeof buf - 1 - len);
		if (n > 0) {
			len += (size_t)n;
			buf[len] = 0;
			return 1;
		}
		return 0;
	}
	return r == 0 ? 1 : 0;
}

static void reap(int block)
{
	int st;
	if (!ended && waitpid(child, &st, block ? 0 : WNOHANG) == child) {
		ended = 1;
		status = st;
	}
}

static void nap(long ms)
{
	struct timespec ts = { ms / 1000, ms % 1000 * 1000000L };
	nanosleep(&ts, NULL);
}

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000;
}

int main(int argc, char *argv[])
{
	int i = 1, opsend, exitcode = 0, timedout = 0;
	char *slave;
	size_t k;

	if (i + 1 < argc && !strcmp(argv[i], "-T")) {
		timeout_s = atoi(argv[i + 1]);
		i += 2;
	}
	if (i + 1 < argc && !strcmp(argv[i], "-o")) {
		rawfile = argv[i + 1];
		i += 2;
	}
	opsend = i;
	while (opsend < argc && strcmp(argv[opsend], "--"))
		opsend++;
	if (opsend >= argc - 1) {
		fprintf(stderr, "usage: ptydrive [-T seconds] [-o file] op ... -- command [argument ...]\n");
		return 2;
	}
	master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0 || grantpt(master) || unlockpt(master) || !(slave = ptsname(master))) {
		perror("ptydrive: pseudo-terminal");
		return 2;
	}
	child = fork();
	if (child == 0) {
		int fd;
		setsid();
		fd = open(slave, O_RDWR);
		if (fd < 0)
			_exit(126);
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
		close(master);
		execvp(argv[opsend + 1], argv + opsend + 1);
		_exit(127);
	}
	for (; i < opsend && !timedout && !exitcode; i++) {
		const char *op = argv[i];
		long t0 = now_ms();
		char text[1024];
		size_t tl;
		if (op[0] == 0 || op[1] != ':') {
			fprintf(stderr, "ptydrive: bad op %s\n", op);
			return 2;
		}
		unescape(op + 2, text, &tl);
		switch (op[0]) {
		case 'e':
			while (!strstr(buf + mark, text)) {
				if (ended || now_ms() - t0 > timeout_s * 1000L) { timedout = 1; break; }
				if (!pump(100)) { reap(0); if (!ended) nap(50); reap(0); if (!strstr(buf + mark, text)) { timedout = 1; break; } }
			}
			if (!timedout)
				mark = (size_t)(strstr(buf + mark, text) - buf) + tl;
			break;
		case 'n':
			while (pump(0) == 1 && now_ms() - t0 < 200)
				;
			if (strstr(buf + mark, text))
				exitcode = 4;
			break;
		case 's':
			(void)!write(master, text, tl); /* (the command may be gone) */
			break;
		case 'd':
			(void)!write(master, "\004", 1);
			break;
		case 't': {
			long until = now_ms() + atol(op + 2);
			while (now_ms() < until)
				pump(20);
			break;
		}
		case 'w':
			while (!ended) {
				if (now_ms() - t0 > timeout_s * 1000L) { timedout = 1; break; }
				pump(50);
				reap(0);
			}
			while (pump(0) == 1 && ended && now_ms() - t0 < 100000)
				if (len >= sizeof buf - 2)
					break;
			break;
		default:
			fprintf(stderr, "ptydrive: bad op %s\n", op);
			return 2;
		}
	}
	/* what is left of the output, then the end */
	for (k = 0; k < 5; k++)
		pump(20);
	reap(0);
	if (rawfile != NULL) {
		FILE *f = fopen(rawfile, "w");
		if (f == NULL || fwrite(buf, 1, len, f) != len || fclose(f) == EOF) {
			perror(rawfile);
			exitcode = 2;
		}
	}
	{
		size_t o = 0;
		for (k = 0; k < len; k++)
			if (buf[k] != '\r')
				buf[o++] = buf[k];
		fwrite(buf, 1, o, stdout);
	}
	if (timedout)
		printf("\n[timeout]\n");
	else if (ended && WIFEXITED(status))
		printf("\n[exit %d]\n", WEXITSTATUS(status));
	else if (ended && WIFSIGNALED(status))
		printf("\n[signal %d]\n", WTERMSIG(status));
	else
		printf("\n[running]\n");
	if (!ended) {
		kill(child, SIGKILL);
		reap(1);
	}
	return timedout ? 3 : exitcode;
}

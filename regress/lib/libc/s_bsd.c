/* The BSD library interfaces the ported BSD sources use: err(3),
 * getprogname(3), arc4random(3) and <sys/param.h>. */
#define _DEFAULT_SOURCE
#include <sys/param.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "out.h"

/* run fn in a child, return what it wrote to stderr in buf and its exit status */
static int capture(void (*fn)(void), char *buf, size_t n)
{
	int p[2], st;
	ssize_t r, len = 0;
	pid_t pid;
	pipe(p);
	pid = fork();
	if (pid == 0) {
		dup2(p[1], 2);
		close(p[0]);
		close(p[1]);
		fn();
		_exit(99);
	}
	close(p[1]);
	while ((r = read(p[0], buf + len, n - 1 - (size_t)len)) > 0) len += r;
	buf[len] = 0;
	close(p[0]);
	waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void f_warn(void) { setprogname("/usr/bin/prog"); errno = ENOENT; warn("open %s", "x"); exit(7); }
static void f_warnx(void) { setprogname("prog"); warnx("value %d", 5); exit(8); }
static void f_err(void) { setprogname("prog"); errno = EACCES; err(3, "denied"); }
static void f_errx(void) { setprogname("prog"); errx(4, "bad"); }
static void f_errc(void) { setprogname("prog"); errno = 0; errc(5, ENOENT, "c %d", 1); }
static void f_warnc(void) { setprogname("prog"); warnc(EACCES, "w"); exit(6); }
static void f_warn_null(void) { setprogname("prog"); errno = EPERM; warn(NULL); exit(9); }

int main(void)
{
	char b[256];
	int i, j, bad, hist[4], same;
	unsigned char x[64], y[64];
	uint32_t a1, a2;

	CHECK(capture(f_warn, b, sizeof b) == 7);
	CHECK(strcmp(b, "prog: open x: No such file or directory\n") == 0);
	CHECK(capture(f_warnx, b, sizeof b) == 8);
	CHECK(strcmp(b, "prog: value 5\n") == 0);
	CHECK(capture(f_err, b, sizeof b) == 3);
	CHECK(strcmp(b, "prog: denied: Permission denied\n") == 0);
	CHECK(capture(f_errx, b, sizeof b) == 4);
	CHECK(strcmp(b, "prog: bad\n") == 0);
	CHECK(capture(f_warn_null, b, sizeof b) == 9);
	CHECK(strcmp(b, "prog: Operation not permitted\n") == 0);

	CHECK(capture(f_errc, b, sizeof b) == 5);
	CHECK(strcmp(b, "prog: c 1: No such file or directory\n") == 0);
	CHECK(capture(f_warnc, b, sizeof b) == 6);
	CHECK(strcmp(b, "prog: w: Permission denied\n") == 0);

	setprogname("a/b/c");
	CHECK(strcmp(getprogname(), "c") == 0);

	/* arc4random: not constant, uniform stays below the bound, buf fills */
	a1 = arc4random();
	same = 0;
	for (i = 0; i < 20; i++) { a2 = arc4random(); if (a2 == a1) same++; }
	CHECK(same < 3);
	bad = 0;
	memset(hist, 0, sizeof hist);
	for (i = 0; i < 4000; i++) {
		uint32_t v = arc4random_uniform(4);
		if (v >= 4) bad++; else hist[v]++;
	}
	CHECK(bad == 0);
	for (i = 0; i < 4; i++) CHECK(hist[i] > 800 && hist[i] < 1200);
	CHECK(arc4random_uniform(0) == 0 && arc4random_uniform(1) == 0);
	for (i = 0; i < 100; i++) CHECK(arc4random_uniform(7) < 7);
	memset(x, 0, sizeof x); memset(y, 0, sizeof y);
	arc4random_buf(x, sizeof x);
	arc4random_buf(y, sizeof y);
	CHECK(memcmp(x, y, sizeof x) != 0);
	for (j = 0, i = 0; i < 64; i++) j += x[i] != 0;
	CHECK(j > 40);
	{ unsigned char big[1000]; memset(big, 0, sizeof big); arc4random_buf(big, sizeof big);
	  for (j = 0, i = 900; i < 1000; i++) j += big[i] != 0; CHECK(j > 70); }

	/* strtonum */
	{ const char *e;
	  CHECK(strtonum("42", 1, 100, &e) == 42 && e == NULL);
	  CHECK(strtonum("0", 1, 100, &e) == 0 && strcmp(e, "too small") == 0 && errno == ERANGE);
	  CHECK(strtonum("101", 1, 100, &e) == 0 && strcmp(e, "too large") == 0);
	  CHECK(strtonum("12x", 1, 100, &e) == 0 && strcmp(e, "invalid") == 0 && errno == EINVAL);
	  CHECK(strtonum("", 1, 100, &e) == 0 && strcmp(e, "invalid") == 0);
	  CHECK(strtonum("5", 9, 1, &e) == 0 && strcmp(e, "invalid") == 0);
	  CHECK(strtonum("-7", -10, 10, NULL) == -7);
	}

	/* <sys/param.h> */
	{ unsigned char m[8] = {0};
	  setbit(m, 13); CHECK(isset(m, 13) && !isset(m, 12) && isclr(m, 14) && m[1] == 0x20);
	  clrbit(m, 13); CHECK(isclr(m, 13)); }
	CHECK(howmany(10, 4) == 3 && howmany(8, 4) == 2);
	CHECK(roundup(5, 4) == 8 && rounddown(7, 4) == 4);
	CHECK(powerof2(64) && !powerof2(96));
	CHECK(DEV_BSIZE == 512 && btodb(4096) == 8 && dbtob(8) == 4096);
	CHECK(MAXPATHLEN >= 1024 && MAXBSIZE == 65536);
	CHECK(MIN(3, 5) == 3 && MAX(3, 5) == 5);
	os(failures ? "bsd FAILED\n" : "bsd ok\n");
	oflush();
	return failures != 0;
}

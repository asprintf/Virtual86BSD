/* Self-checking: realpath, mkstemp/mkostemp/mkdtemp, system, getsubopt,
 * the pseudo-terminal functions and the wait macros from <stdlib.h>. */
#define _XOPEN_SOURCE 800
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <termios.h>
#include "out.h"

int main(void)
{
	char dir[64] = "/tmp/libc-sstdlib.XXXXXX", buf[PATH_MAX], t[PATH_MAX], cwd[PATH_MAX], *p, *v;
	int fd, st, i, fl;
	struct stat sb;

	/* mkdtemp, mkstemp, mkostemp */
	CHECK(mkdtemp(dir) == dir && strncmp(dir, "/tmp/libc-sstdlib.", 18) == 0 && strcmp(dir + 18, "XXXXXX") != 0);
	CHECK(stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode) && (sb.st_mode & 0777) == 0700);
	strcpy(buf, "bad"); CHECK(mkdtemp(buf) == 0 && errno == EINVAL);
	strcpy(buf, "XXXXX"); CHECK(mkstemp(buf) == -1 && errno == EINVAL);
	CHECK(getcwd(cwd, sizeof cwd) != 0);
	CHECK(chdir(dir) == 0);
	strcpy(buf, "fXXXXXX"); fd = mkstemp(buf);
	CHECK(fd >= 0 && fstat(fd, &sb) == 0 && S_ISREG(sb.st_mode) && (sb.st_mode & 0777) == 0600);
	CHECK(write(fd, "x", 1) == 1); close(fd);
	strcpy(t, "gXXXXXX"); fd = mkostemp(t, O_CLOEXEC | O_APPEND);
	CHECK(fd >= 0 && strcmp(t, buf) != 0);
	fl = fcntl(fd, F_GETFD); CHECK(fl & FD_CLOEXEC);
	fl = fcntl(fd, F_GETFL); CHECK(fl & O_APPEND);
	close(fd);
	for (i = 0; i < 50; i++) {    /* uniqueness under repetition */
		strcpy(t, "uXXXXXX"); fd = mkstemp(t); CHECK(fd >= 0); close(fd);
	}

	/* realpath */
	CHECK(mkdir("d", 0777) == 0 && mkdir("d/e", 0777) == 0);
	CHECK(symlink("d/e", "l") == 0 && symlink("../..", "d/e/up") == 0);
	CHECK(symlink("loop2", "loop1") == 0 && symlink("loop1", "loop2") == 0);
	CHECK(symlink("nonexistent", "dangling") == 0);
	CHECK(realpath(".", t) == t && !strcmp(t, dir));
	p = realpath("l/./../e//up/d/", 0);
	strcpy(t, dir); strcat(t, "/d");
	CHECK(p && !strcmp(p, t)); free(p);
	CHECK(realpath("/", t) && !strcmp(t, "/"));
	CHECK(realpath("//..//.", t) && !strcmp(t, "/"));
	CHECK(realpath(buf, t) && !strncmp(t, dir, strlen(dir)) && !strcmp(t + strlen(dir) + 1, buf));
	CHECK(realpath("nothere", t) == 0 && errno == ENOENT);
	CHECK(realpath("dangling", t) == 0 && errno == ENOENT);
	CHECK(realpath("loop1", t) == 0 && errno == ELOOP);
	{ char f[16] = "f"; strcat(f, buf + 1); strcat(f, "/");
	  CHECK(realpath(f, t) == 0 && errno == ENOTDIR); }
	CHECK(realpath("", t) == 0 && errno == ENOENT);
	errno = 0; CHECK(realpath(0, t) == 0 && errno == EINVAL);

	/* system */
	CHECK(system(0) != 0);
	st = system("exit 3"); CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 3);
	st = system("true"); CHECK(st == 0);
	st = system("echo hi > sysout"); CHECK(st == 0);
	fd = open("sysout", O_RDONLY); CHECK(fd >= 0 && read(fd, t, 10) == 3 && !memcmp(t, "hi\n", 3)); close(fd);
	st = system("kill -TERM $$"); CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM);
	{ struct sigaction sa; sigaction(SIGINT, 0, &sa); CHECK(sa.sa_handler == SIG_DFL); }

	/* getsubopt */
	{
		char *const tok[] = { "ro", "rw", "size", 0 };
		char opts[] = "rw,size=10,bogus=1,ro", *o = opts;
		CHECK(getsubopt(&o, tok, &v) == 1 && v == 0);
		CHECK(getsubopt(&o, tok, &v) == 2 && v && !strcmp(v, "10"));
		CHECK(getsubopt(&o, tok, &v) == -1 && v && !strcmp(v, "bogus=1"));
		CHECK(getsubopt(&o, tok, &v) == 0 && v == 0);
		CHECK(*o == 0);
	}

	/* pseudo-terminals */
	fd = posix_openpt(O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		int s;
		CHECK(grantpt(fd) == 0 && unlockpt(fd) == 0);
		p = ptsname(fd); CHECK(p && !strncmp(p, "/dev/pts/", 9));
		CHECK(ptsname_r(fd, t, sizeof t) == 0 && !strcmp(t, p));
		CHECK(ptsname_r(fd, t, 3) == ERANGE);
		s = open(t, O_RDWR | O_NOCTTY); CHECK(s >= 0);
		CHECK(isatty(s));
		{
			/* tcgetattr fills the whole structure (the kernel only 19 c_cc) */
			struct termios a, b;
			int k, z = 1;
			memset(&a, 0x55, sizeof a);
			memset(&b, 0xaa, sizeof b);
			CHECK(tcgetattr(s, &a) == 0 && tcgetattr(s, &b) == 0 && !memcmp(&a, &b, sizeof a));
			for (k = 19; k < NCCS; k++) if (a.c_cc[k]) z = 0;
			CHECK(z && cfgetospeed(&a) == cfgetospeed(&b));
			b.c_iflag |= BRKINT;
			CHECK(tcsetattr(s, TCSANOW, &b) == 0 && tcgetattr(s, &a) == 0 && !memcmp(&a, &b, sizeof a));
		}
		close(s); close(fd);
	}
	CHECK(ptsname_r(999, t, sizeof t) != 0);
	CHECK(grantpt(999) == -1 && errno == EBADF);

	/* cleanup */
	/* a64l sign-extends (POSIX); glibc returns the unsigned value */
	CHECK(a64l("zzzzz1") == -1);
	CHECK(chdir(cwd) == 0);
	strcpy(t, "rm -rf "); strcat(t, dir);
	CHECK(system(t) == 0 && stat(dir, &sb) == -1);
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}

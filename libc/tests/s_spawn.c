/* Self-checking: setjmp/sigsetjmp, posix_spawn, wordexp, nftw, fnmatch
 * with FNM_CASEFOLD. */
#define _XOPEN_SOURCE 800
#include <setjmp.h>
#include <signal.h>
#include <spawn.h>
#include <wordexp.h>
#include <ftw.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sched.h>
#include "out.h"

extern char **environ;
static jmp_buf jb;
static sigjmp_buf sjb;
static volatile int depth;

static void deep(int n) { if (n == 0) longjmp(jb, 42); deep(n - 1); depth++; }
static void onusr1(int s) { (void)s; siglongjmp(sjb, 7); }

static char seen[4096];
static int nseen;
static int visit(const char *p, const struct stat *st, int type, struct FTW *f)
{
	(void)st;
	if (strlen(seen) + strlen(p) + 16 < sizeof seen) {
		char t[4] = " ??";
		t[1] = "?FDRNLPS"[type];
		t[2] = (char)('0' + f->level);
		strcat(seen, t);
		strcat(seen, p + f->base);
	}
	nseen++;
	return 0;
}
static int stop(const char *p, const struct stat *st, int type, struct FTW *f)
{ (void)p; (void)st; (void)type; (void)f; return ++nseen == 3 ? 99 : 0; }

int main(int argc, char **argv)
{
	char dir[64] = "/tmp/libc-sspawn.XXXXXX", cwd[1024], buf[256];
	volatile int v = 1;
	int r, st, p[2];
	pid_t pid;
	sigset_t set;

	/* run by the scheduling test below: report policy and priority */
	if (argc > 1 && !strcmp(argv[1], "sched")) {
		struct sched_param sp;
		sched_getparam(0, &sp);
		return sched_getscheduler(0) * 100 + sp.sched_priority;
	}

	/* setjmp / longjmp */
	r = setjmp(jb);
	if (r == 0) { v = 2; deep(50); CHECK(0); }
	CHECK(r == 42 && v == 2 && depth == 0);
	if ((r = setjmp(jb)) == 0) longjmp(jb, 0);
	CHECK(r == 1);
	/* sigsetjmp: the mask comes back only when asked for */
	signal(SIGUSR1, onusr1);
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, 0);
	if ((r = sigsetjmp(sjb, 1)) == 0) raise(SIGUSR1);
	sigprocmask(SIG_SETMASK, 0, &set);
	CHECK(r == 7 && !sigismember(&set, SIGUSR1));
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, 0);
	if ((r = sigsetjmp(sjb, 0)) == 0) raise(SIGUSR1);
	sigprocmask(SIG_SETMASK, 0, &set);
	CHECK(r == 7 && sigismember(&set, SIGUSR1));   /* still blocked, as in the handler */
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, 0);

	CHECK(mkdtemp(dir) != 0 && getcwd(cwd, sizeof cwd) != 0 && chdir(dir) == 0);

	/* posix_spawn */
	{
		char *argv[] = { "sh", "-c", "echo out; echo err >&2; exit 3", 0 };
		char *argv2[] = { "sh", "-c", "pwd; kill -0 $$; echo $(($$ == $(ps -o pgid= -p $$)))", 0 };
		char *argv3[] = { "true", 0 };
		posix_spawn_file_actions_t fa;
		posix_spawnattr_t at;
		short fl;
		CHECK(posix_spawn_file_actions_init(&fa) == 0);
		CHECK(posix_spawn_file_actions_addopen(&fa, 1, "out", O_WRONLY | O_CREAT | O_TRUNC, 0644) == 0);
		CHECK(posix_spawn_file_actions_adddup2(&fa, 1, 2) == 0);
		CHECK(posix_spawn_file_actions_addclose(&fa, 0) == 0);
		CHECK(posix_spawn_file_actions_addclose(&fa, -1) == EBADF);
		CHECK(posix_spawn(&pid, "/bin/sh", &fa, 0, argv, environ) == 0);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 3);
		posix_spawn_file_actions_destroy(&fa);
		r = open("out", O_RDONLY); CHECK(read(r, buf, sizeof buf) == 8 && !memcmp(buf, "out\nerr\n", 8)); close(r);

		CHECK(posix_spawnp(&pid, "nonexistent-command-xyz", 0, 0, argv3, environ) == ENOENT);
		CHECK(posix_spawn(&pid, "/nonexistent", 0, 0, argv3, environ) == ENOENT);
		CHECK(posix_spawnp(&pid, "true", 0, 0, argv3, environ) == 0);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);

		/* chdir action, new process group, signal mask and defaults */
		CHECK(mkdir("sub", 0755) == 0);
		posix_spawn_file_actions_init(&fa);
		posix_spawn_file_actions_addchdir(&fa, "sub");
		posix_spawn_file_actions_addopen(&fa, 1, "../out2", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		posix_spawnattr_init(&at);
		CHECK(posix_spawnattr_setflags(&at, 0x4000) == EINVAL);
		CHECK(posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK) == 0);
		posix_spawnattr_getflags(&at, &fl); CHECK(fl == (POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK));
		posix_spawnattr_setpgroup(&at, 0);
		sigemptyset(&set); sigaddset(&set, SIGUSR2);
		posix_spawnattr_setsigmask(&at, &set);
		CHECK(posix_spawn(&pid, "/bin/sh", &fa, &at, argv2, environ) == 0);
		CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		r = open("out2", O_RDONLY); buf[read(r, buf, sizeof buf - 1)] = 0; close(r);
		CHECK(strstr(buf, "/sub\n") != 0);
		CHECK(strstr(buf, "\n1\n") != 0);     /* its own process group */
		posix_spawnattr_destroy(&at);
		posix_spawn_file_actions_destroy(&fa);

		/* [PS] scheduling attributes */
		{
			char *argv5[] = { argv[0], "sched", 0 };
			struct sched_param sp = { .sched_priority = 5 }, sq;
			int pol, fifo_ok;
			posix_spawnattr_init(&at);
			CHECK(posix_spawnattr_setschedpolicy(&at, 999) == EINVAL);
			CHECK(posix_spawnattr_setschedpolicy(&at, SCHED_FIFO) == 0);
			CHECK(posix_spawnattr_setschedparam(&at, &sp) == 0);
			posix_spawnattr_getschedpolicy(&at, &pol);
			posix_spawnattr_getschedparam(&at, &sq);
			CHECK(pol == SCHED_FIFO && sq.sched_priority == 5);
			/* may a process of ours become SCHED_FIFO? ask in a child */
			if ((pid = fork()) == 0) _exit(sched_setscheduler(0, SCHED_FIFO, &sp) < 0);
			waitpid(pid, &st, 0);
			fifo_ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
			CHECK(posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSCHEDULER) == 0);
			r = posix_spawn(&pid, "/proc/self/exe", 0, &at, argv5, environ);
			if (fifo_ok) {
				CHECK(r == 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == SCHED_FIFO * 100 + 5);
			} else
				CHECK(r == EPERM);
			/* the policy alone: SCHED_OTHER with priority 0 */
			sp.sched_priority = 0;
			posix_spawnattr_setschedpolicy(&at, SCHED_OTHER);
			posix_spawnattr_setschedparam(&at, &sp);
			CHECK(posix_spawn(&pid, "/proc/self/exe", 0, &at, argv5, environ) == 0);
			CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
			/* parameters only, keeping our SCHED_OTHER: priority 50 is invalid there */
			sp.sched_priority = 50;
			posix_spawnattr_setschedparam(&at, &sp);
			posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSCHEDPARAM);
			CHECK(posix_spawn(&pid, "/proc/self/exe", 0, &at, argv5, environ) == EINVAL);
			sp.sched_priority = 0;
			posix_spawnattr_setschedparam(&at, &sp);
			CHECK(posix_spawn(&pid, "/proc/self/exe", 0, &at, argv5, environ) == 0);
			CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
			posix_spawnattr_destroy(&at);
		}

		/* the child gets no handler of ours; it sees the descriptor
		 * of a pipe only without FD_CLOEXEC */
		CHECK(pipe(p) == 0);
		fcntl(p[0], F_SETFD, FD_CLOEXEC);
		{
			char a1[32], *argv4[] = { "sh", "-c", a1, 0 };
			char *q = a1;
			strcpy(q, "exec 2>/dev/null; : <&"); q += strlen(q);
			*q++ = (char)('0' + p[0] / 10 % 10); *q++ = (char)('0' + p[0] % 10); *q = 0;
			if (p[0] < 10) { a1[strlen(a1) - 2] = a1[strlen(a1) - 1]; a1[strlen(a1) - 1] = 0; }
			CHECK(posix_spawn(&pid, "/bin/sh", 0, 0, argv4, environ) == 0);
			CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) != 0);
		}
		close(p[0]); close(p[1]);
	}

	/* wordexp */
	{
		wordexp_t we;
		setenv("WX", "one two", 1);
		unsetenv("WUNDEF");
		CHECK(wordexp("a \"b c\" $WX 'd e' \\f", &we, 0) == 0 && we.we_wordc == 6);
		if (we.we_wordc == 6)
			CHECK(!strcmp(we.we_wordv[0], "a") && !strcmp(we.we_wordv[1], "b c") && !strcmp(we.we_wordv[2], "one") &&
			      !strcmp(we.we_wordv[4], "d e") && !strcmp(we.we_wordv[5], "f") && we.we_wordv[6] == 0);
		CHECK(wordexp("$((1+2)) $(echo x)", &we, WRDE_REUSE) == 0 && we.we_wordc == 2 &&
		      !strcmp(we.we_wordv[0], "3") && !strcmp(we.we_wordv[1], "x"));
		CHECK(wordexp("s*b", &we, WRDE_APPEND) == 0 && we.we_wordc == 3 && !strcmp(we.we_wordv[2], "sub"));
		wordfree(&we);
		CHECK(wordexp("$(echo x)", &we, WRDE_NOCMD) == WRDE_CMDSUB);
		CHECK(wordexp("`echo x`", &we, WRDE_NOCMD) == WRDE_CMDSUB);
		CHECK(wordexp("a;b", &we, 0) == WRDE_BADCHAR);
		CHECK(wordexp("a|b", &we, 0) == WRDE_BADCHAR);
		CHECK(wordexp("\"a;b\"", &we, 0) == 0 && we.we_wordc == 1 && !strcmp(we.we_wordv[0], "a;b"));
		wordfree(&we);
		CHECK(wordexp("'abc", &we, 0) == WRDE_SYNTAX);
		CHECK(wordexp("$WUNDEF", &we, WRDE_UNDEF) == WRDE_BADVAL);
		we.we_offs = 2;
		CHECK(wordexp("x y", &we, WRDE_DOOFFS) == 0 && we.we_wordc == 2 && !we.we_wordv[0] && !we.we_wordv[1] &&
		      !strcmp(we.we_wordv[3], "y"));
		wordfree(&we);
	}

	/* nftw */
	{
		CHECK(mkdir("t", 0755) == 0 && mkdir("t/d", 0755) == 0 && mkdir("t/e", 0) == 0);
		close(open("t/d/f", O_CREAT | O_WRONLY, 0644));
		CHECK(symlink("d", "t/l") == 0 && symlink("nowhere", "t/x") == 0);
		CHECK(nftw("t", visit, 4, FTW_PHYS) == 0);
		/* order within a directory is the directory's own, so look for pieces */
		CHECK(strstr(seen, " D0t") && strstr(seen, " D1d") && strstr(seen, " F2f") && strstr(seen, " L1l") &&
		      strstr(seen, " L1x") && (strstr(seen, " R1e") || (geteuid() == 0 && strstr(seen, " D1e"))));
		seen[0] = 0; nseen = 0;
		CHECK(nftw("t", visit, 4, FTW_DEPTH) == 0);
		CHECK(strstr(seen, " P0t") && strstr(seen, " P1d") && strstr(seen, " S1x") && !strstr(seen, " L1l") &&
		      strstr(seen, " P1l"));
		CHECK(strstr(seen, " P1d") > strstr(seen, " F2f"));
		seen[0] = 0; nseen = 0;
		CHECK(nftw("t/", visit, 4, FTW_CHDIR | FTW_PHYS) == 0 && strstr(seen, " F2f"));
		{ char here[1024]; CHECK(getcwd(here, sizeof here) && !strcmp(here + strlen(here) - strlen(dir + 4), dir + 4)); }
		nseen = 0;
		CHECK(nftw("t", stop, 4, 0) == 99 && nseen == 3);
		errno = 0;
		CHECK(nftw("nonexistent", visit, 4, 0) == -1 && errno == ENOENT);
	}

	/* fnmatch: case folding */
	CHECK(fnmatch("A*[B-C]", "abc", FNM_CASEFOLD) == 0);
	CHECK(fnmatch("A*[B-C]", "abc", 0) == FNM_NOMATCH);
	CHECK(fnmatch("[[:upper:]]", "a", FNM_IGNORECASE) == 0);
	CHECK(fnmatch("*.c", ".x.c", FNM_PERIOD) == FNM_NOMATCH);
	CHECK(fnmatch("a/*", "a/.x", FNM_PERIOD | FNM_PATHNAME) == FNM_NOMATCH);
	CHECK(fnmatch("a/.*", "a/.x", FNM_PERIOD | FNM_PATHNAME) == 0);
	CHECK(fnmatch("a[b/c]d", "a/d", FNM_PATHNAME) == FNM_NOMATCH);
	CHECK(fnmatch("a[b/c]d", "a[b/c]d", FNM_PATHNAME) == 0);

	CHECK(chdir(cwd) == 0);
	{ char cmd[100] = "chmod 755 "; strcat(cmd, dir); strcat(cmd, "/t/e; rm -rf "); strcat(cmd, dir); CHECK(system(cmd) == 0); }
	os(failures ? "failures\n" : "ok\n");
	oflush();
	return failures != 0;
}

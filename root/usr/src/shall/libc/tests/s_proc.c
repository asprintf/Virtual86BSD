/* Processes and signals: fork, exec, wait, kill, sigaction, masks, queues. */
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "out.h"

static volatile sig_atomic_t got, got_code, got_val, got_pid;
static void on_usr1(int s) { got = s; }
static void on_info(int s, siginfo_t *si, void *uc)
{ (void)uc; got = s; got_code = si->si_code; got_val = si->si_value.sival_int; got_pid = si->si_pid; }

int main(void)
{
	pid_t pid;
	int st, fds[2], sig;
	struct sigaction sa, old;
	sigset_t set, oset;
	siginfo_t si;
	struct timespec ts;
	char buf[64];
	union sigval v;
	struct utsname u;
	struct rlimit rl;
	struct rusage ru;
	struct tms tm;
	struct pollfd pf;
	fd_set rs;
	struct timeval tv;
	char *mp;

	/* exit status, signals as status, _Fork, waitid */
	pid = fork();
	if (!pid) _exit(42);
	CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 42 && !WIFSIGNALED(st));
	pid = _Fork();
	if (!pid) { kill(getpid(), SIGTERM); _exit(1); }
	CHECK(wait(&st) == pid && WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM && !WIFEXITED(st));
	pid = fork();
	if (!pid) { raise(SIGSTOP); _exit(7); }
	CHECK(waitpid(pid, &st, WUNTRACED) == pid && WIFSTOPPED(st) && WSTOPSIG(st) == SIGSTOP);
	CHECK(kill(pid, SIGCONT) == 0);
	CHECK(waitid(P_PID, (id_t)pid, &si, WEXITED) == 0 && si.si_pid == pid &&
	      si.si_code == CLD_EXITED && si.si_status == 7 && si.si_signo == SIGCHLD);
	CHECK(waitpid(-1, &st, WNOHANG) == -1 && errno == ECHILD);

	/* exec family */
	pid = fork();
	if (!pid) { execl("/bin/sh", "sh", "-c", "exit 3", (char *)0); _exit(99); }
	CHECK(waitpid(pid, &st, 0) == pid && WEXITSTATUS(st) == 3);
	pid = fork();
	if (!pid) { char *a[] = { "sh", "-c", "test \"$X\" = y", 0 }, *e[] = { "X=y", 0 }; execve("/bin/sh", a, e); _exit(99); }
	CHECK(waitpid(pid, &st, 0) == pid && WEXITSTATUS(st) == 0);
	pid = fork();
	if (!pid) { execlp("sh", "sh", "-c", "exit 5", (char *)0); _exit(99); }
	CHECK(waitpid(pid, &st, 0) == pid && WEXITSTATUS(st) == 5);
	pid = fork();
	if (!pid) { char *a[] = { "nonexistent-cmd-xyz", 0 }; execvp(a[0], a); _exit(errno == ENOENT ? 4 : 99); }
	CHECK(waitpid(pid, &st, 0) == pid && WEXITSTATUS(st) == 4);
	CHECK(pipe(fds) == 0);
	pid = fork();
	if (!pid) { dup2(fds[1], 1); close(fds[0]); close(fds[1]); execle("/bin/sh", "sh", "-c", "printf %s \"$V\"", (char *)0, (char *[]){ "V=env-ok", 0 }); _exit(99); }
	close(fds[1]);
	CHECK(read(fds[0], buf, sizeof buf) == 6 && !memcmp(buf, "env-ok", 6));
	close(fds[0]);
	CHECK(waitpid(pid, &st, 0) == pid && WEXITSTATUS(st) == 0);

	/* sigaction, raise, masks */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_usr1;
	CHECK(sigemptyset(&sa.sa_mask) == 0 && sigaction(SIGUSR1, &sa, &old) == 0 && old.sa_handler == SIG_DFL);
	CHECK(raise(SIGUSR1) == 0 && got == SIGUSR1);
	CHECK(sigaction(SIGUSR1, 0, &old) == 0 && old.sa_handler == on_usr1);
	CHECK(sigaction(SIGKILL, &sa, 0) == -1 && errno == EINVAL);
	CHECK(sigaction(33, &sa, 0) == -1 && errno == EINVAL);
	CHECK(signal(SIGUSR2, SIG_IGN) == SIG_DFL && signal(SIGUSR2, SIG_DFL) == SIG_IGN);
	CHECK(signal(SIGKILL, SIG_IGN) == SIG_ERR);
	sigemptyset(&set); sigaddset(&set, SIGUSR1);
	CHECK(sigprocmask(SIG_BLOCK, &set, &oset) == 0 && !sigismember(&oset, SIGUSR1));
	got = 0;
	CHECK(kill(getpid(), SIGUSR1) == 0 && got == 0);
	CHECK(sigpending(&oset) == 0 && sigismember(&oset, SIGUSR1) == 1);
	sigemptyset(&oset);
	CHECK(sigsuspend(&oset) == -1 && errno == EINTR && got == SIGUSR1);
	CHECK(sigprocmask(SIG_UNBLOCK, &set, 0) == 0);
	CHECK(sigprocmask(99, &set, 0) == -1 && errno == EINVAL);
	CHECK(pthread_sigmask(99, &set, 0) == EINVAL);

	/* realtime signals with values; sigwaitinfo, sigtimedwait, sigwait */
	sa.sa_sigaction = on_info; sa.sa_flags = SA_SIGINFO;
	CHECK(sigaction(SIGRTMIN, &sa, 0) == 0);
	v.sival_int = 1234; got = 0;
	CHECK(sigqueue(getpid(), SIGRTMIN, v) == 0 && got == SIGRTMIN && got_code == SI_QUEUE &&
	      got_val == 1234 && got_pid == getpid());
	sigemptyset(&set); sigaddset(&set, SIGRTMIN + 1); sigaddset(&set, SIGUSR2);
	sigprocmask(SIG_BLOCK, &set, 0);
	v.sival_int = 77;
	CHECK(sigqueue(getpid(), SIGRTMIN + 1, v) == 0);
	CHECK(sigwaitinfo(&set, &si) == SIGRTMIN + 1 && si.si_value.sival_int == 77 && si.si_code == SI_QUEUE);
	ts.tv_sec = 0; ts.tv_nsec = 1000000;
	CHECK(sigtimedwait(&set, &si, &ts) == -1 && errno == EAGAIN);
	kill(getpid(), SIGUSR2);
	CHECK(sigwait(&set, &sig) == 0 && sig == SIGUSR2);
	sigprocmask(SIG_UNBLOCK, &set, 0);

	/* sigset rules for reserved signals */
	sigfillset(&set);
	CHECK(sigismember(&set, 1) == 1 && sigismember(&set, 31) == 1 && sigismember(&set, 32) == 0 &&
	      sigismember(&set, 34) == 0 && sigismember(&set, 35) == 1 && sigismember(&set, 64) == 1);
	CHECK(sigaddset(&set, 33) == -1 && errno == EINVAL && sigaddset(&set, 65) == -1 && sigismember(&set, 0) == -1);

	/* sig2str / str2sig */
	CHECK(sig2str(SIGINT, buf) == 0 && !strcmp(buf, "INT"));
	CHECK(sig2str(SIGRTMIN, buf) == 0 && !strcmp(buf, "RTMIN"));
	CHECK(sig2str(SIGRTMIN + 3, buf) == 0 && !strcmp(buf, "RTMIN+3"));
	CHECK(sig2str(SIGRTMAX, buf) == 0 && !strcmp(buf, "RTMAX"));
	CHECK(sig2str(SIGRTMAX - 2, buf) == 0 && !strcmp(buf, "RTMAX-2"));
	CHECK(sig2str(33, buf) == -1);
	for (sig = 1; sig < 65; sig++) {
		int back;
		if (sig >= 32 && sig < 35) continue;
		if (sig2str(sig, buf) || str2sig(buf, &back) || back != sig) { CHECK(!"sig2str round trip"); break; }
	}
	CHECK(str2sig("15", &sig) == 0 && sig == 15 && str2sig("HUP", &sig) == 0 && sig == 1);
	CHECK(str2sig("SIGHUP", &sig) == -1 && str2sig("RTMIN+99", &sig) == -1 && str2sig("33", &sig) == -1);

	/* alarm + pause, sleep */
	signal(SIGALRM, on_usr1); got = 0;
	CHECK(alarm(1) == 0 && pause() == -1 && errno == EINTR && got == SIGALRM);
	CHECK(sleep(0) == 0);

	/* misc */
	CHECK(uname(&u) == 0 && !strcmp(u.sysname, "Linux"));
	CHECK(gethostname(buf, sizeof buf) == 0 && !strcmp(buf, u.nodename));
	CHECK(gethostname(buf, 1) == -1 && errno == ENAMETOOLONG);
	CHECK(getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur > 0 && setrlimit(RLIMIT_NOFILE, &rl) == 0);
	CHECK(getrusage(RUSAGE_SELF, &ru) == 0);
	CHECK(times(&tm) != (clock_t)-1);
	CHECK(nice(0) >= -20);
	CHECK(getpriority(PRIO_PROCESS, 0) == nice(0));
	CHECK(getentropy(buf, 32) == 0 && getentropy(buf, 257) == -1 && errno == EINVAL);
	CHECK(getppid() > 0 && getpgrp() == getpgid(0) && getsid(0) > 0);
	CHECK(sysconf(_SC_VERSION) == 202405L && sysconf(_SC_PAGESIZE) == 4096 &&
	      sysconf(_SC_OPEN_MAX) > 0 && sysconf(_SC_NPROCESSORS_ONLN) >= 1 &&
	      sysconf(_SC_ARG_MAX) >= _POSIX_ARG_MAX && sysconf(_SC_CLK_TCK) == 100);
	CHECK(sysconf(-5) == -1 && errno == EINVAL);
	CHECK(confstr(_CS_PATH, buf, sizeof buf) == 14 && !strcmp(buf, "/bin:/usr/bin"));
	CHECK(confstr(_CS_PATH, buf, 4) == 14 && !strcmp(buf, "/bi"));
	{ uid_t r, e, s; CHECK(getresuid(&r, &e, &s) == 0 && r == getuid() && e == geteuid()); }

	/* poll, select */
	CHECK(pipe(fds) == 0);
	pf.fd = fds[0]; pf.events = POLLIN;
	CHECK(poll(&pf, 1, 0) == 0);
	write(fds[1], "x", 1);
	CHECK(poll(&pf, 1, -1) == 1 && (pf.revents & POLLIN));
	FD_ZERO(&rs); FD_SET(fds[0], &rs);
	tv.tv_sec = 0; tv.tv_usec = 0;
	CHECK(select(fds[0] + 1, &rs, 0, 0, &tv) == 1 && FD_ISSET(fds[0], &rs));
	close(fds[0]); close(fds[1]);

	/* mmap */
	mp = mmap(0, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	CHECK(mp != MAP_FAILED);
	if (mp != MAP_FAILED) {
		mp[8191] = 1;
		CHECK(mprotect(mp, 4096, PROT_READ) == 0 && posix_madvise(mp, 8192, POSIX_MADV_DONTNEED) == 0 && mp[8191] == 1);
		CHECK(munmap(mp, 8192) == 0);
	}
	CHECK(mmap(0, 4096, PROT_READ, MAP_PRIVATE, -1, 0) == MAP_FAILED && errno == EBADF);
	{
		int fd = shm_open("/libc-test-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) && ftruncate(fd, 4096) == 0);
			CHECK(shm_unlink("/libc-test-shm") == 0 && close(fd) == 0);
		}
		CHECK(shm_open("a/b", O_RDONLY, 0) == -1 && errno == EINVAL);
	}
	os(failures ? "proc FAILED\n" : "proc ok\n");
	oflush();
	return failures != 0;
}

/* An address no user program can run at (the return address of
 * rt_sigreturn, an ELF entry point, a signal handler) must end the process
 * with SIGSEGV; it must not stop the kernel (iretq faults in kernel mode on a
 * non-canonical one). */
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

#define GREG_RIP 16 /* index of rip in the sigcontext */

static unsigned long long newrip;

static void handler(int sig, siginfo_t *si, void *ctx)
{
	ucontext_t *uc = ctx;

	(void)sig;
	(void)si;
	uc->uc_mcontext.__gregs[GREG_RIP] = (long long)newrip;
}

/* the child: leave a signal handler that changed the return address */
static void sigret_child(unsigned long long rip)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, NULL);
	newrip = rip;
	raise(SIGUSR1);
	_exit(0);
}

/* the child: a signal handler at that address */
static void handler_child(unsigned long long addr)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = (void (*)(int))addr;
	sigaction(SIGUSR1, &sa, NULL);
	raise(SIGUSR1);
	_exit(0);
}

static void put16(unsigned char *p, unsigned v) { p[0] = v & 255; p[1] = v >> 8; }
static void put32(unsigned char *p, unsigned long v) { put16(p, v & 0xffff); put16(p + 2, (v >> 16) & 0xffff); }
static void put64(unsigned char *p, unsigned long long v) { put32(p, v & 0xffffffffUL); put32(p + 4, v >> 32); }

/* a static executable with one load segment, entering at rip */
static int write_elf(const char *path, unsigned long long rip)
{
	unsigned char b[120];
	int fd;

	memset(b, 0, sizeof(b));
	memcpy(b, "\177ELF", 4);
	b[4] = 2, b[5] = 1, b[6] = 1;
	put16(b + 16, 2);	/* ET_EXEC */
	put16(b + 18, 62);	/* x86-64 */
	put32(b + 20, 1);
	put64(b + 24, rip);	/* entry */
	put64(b + 32, 64);	/* program headers */
	put16(b + 52, 64);
	put16(b + 54, 56);
	put16(b + 56, 1);
	put16(b + 58, 64);
	put32(b + 64, 1);	/* PT_LOAD, R+X */
	put32(b + 68, 5);
	put64(b + 80, 0x400000);
	put64(b + 88, 0x400000);
	put64(b + 96, sizeof(b));
	put64(b + 104, sizeof(b));
	put64(b + 112, 0x1000);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		return -1;
	if (write(fd, b, sizeof(b)) != (ssize_t)sizeof(b)) {
		close(fd);
		return -1;
	}
	return close(fd);
}

int main(void)
{
	static const unsigned long long rips[] = {
		0x0000800000000000ULL,	/* the first non-canonical address */
		0x8000000000000000ULL,
		0x00ff000000000000ULL,
		0xffff800000000000ULL,	/* canonical, in the kernel half */
		0xffffffff80000000ULL,	/* the kernel's text */
	};
	char path[64];
	int st;
	unsigned i;

	for (i = 0; i < sizeof(rips) / sizeof(rips[0]); i++) {
		pid_t pid;

		printf("sigreturn to %#llx\n", rips[i]);
		fflush(stdout);
		pid = fork();
		if (pid == 0)
			sigret_child(rips[i]);
		CHECK(waitpid(pid, &st, 0) == pid);
		CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV);
	}

	for (i = 0; i < sizeof(rips) / sizeof(rips[0]); i++) {
		pid_t pid;

		printf("signal handler at %#llx\n", rips[i]);
		fflush(stdout);
		pid = fork();
		if (pid == 0)
			handler_child(rips[i]);
		CHECK(waitpid(pid, &st, 0) == pid);
		CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV);
	}

	snprintf(path, sizeof(path), "/tmp/k_badrip.%ld", (long)getpid());
	for (i = 0; i < sizeof(rips) / sizeof(rips[0]); i++) {
		pid_t pid;

		printf("exec entry %#llx\n", rips[i]);
		fflush(stdout);
		CHECK(write_elf(path, rips[i]) == 0);
		pid = fork();
		if (pid == 0) {
			execl(path, path, (char *)NULL);
			_exit(77);	/* exec refused the file */
		}
		CHECK(waitpid(pid, &st, 0) == pid);
		CHECK((WIFEXITED(st) && WEXITSTATUS(st) == 77) || (WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV));
	}
	unlink(path);

	printf(failures ? "k_badrip FAILED\n" : "k_badrip ok\n");
	return failures != 0;
}

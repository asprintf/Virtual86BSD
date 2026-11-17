/* segvdbg.c - linked into a test by vmrun.sh -g: a crash prints the program counter, the
 * address and the code addresses found on the stack (resolve with addr2line/nm on the host) */
#define _DEFAULT_SOURCE
#include <signal.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static void hex(unsigned long v)
{
	char b[20]; int i = 0;
	do b[i++] = "0123456789abcdef"[v & 15]; while (v >>= 4);
	write(2, "0x", 2);
	while (i) write(2, &b[--i], 1);
}

extern char __executable_start[], etext[];

static void on_segv(int sig, siginfo_t *si, void *ctx)
{
	ucontext_t *uc = ctx;
	unsigned long *sp = (unsigned long *)uc->uc_mcontext.__mc_rsp;
	int i, n = 0;
	write(2, "SEGV rip=", 9); hex((unsigned long)uc->uc_mcontext.__mc_rip);
	write(2, " addr=", 6); hex((unsigned long)si->si_addr);
	write(2, " sig=", 5); hex((unsigned long)sig);
	write(2, "\nSTACK", 6);
	for (i = 0; i < 4096 && n < 24; i++) {
		unsigned long v;
		if ((unsigned long)(sp + i) & 0xfff && 0) break;
		v = sp[i];
		if (v >= (unsigned long)__executable_start && v < (unsigned long)etext) { write(2, " ", 1); hex(v); n++; }
	}
	write(2, "\n", 1);
	_exit(139);
}

__attribute__((constructor)) static void install(void)
{
	static char altstack[16384];
	stack_t ss = { altstack, sizeof altstack, 0 };
	struct sigaction sa;
	sigaltstack(&ss, 0);
	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = on_segv;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
}

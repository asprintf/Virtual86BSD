#include <signal.h>
#include <string.h>
#include <errno.h>

static const char names[][8] = {
	"", "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1",
	"SEGV", "USR2", "PIPE", "ALRM", "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP",
	"TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "POLL", "PWR", "SYS",
};

int sig2str(int sig, char *s)
{
	if (sig > 0 && sig < 32) { strcpy(s, names[sig]); return 0; }
	if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
		/* RTMIN, RTMIN+n for the lower half, RTMAX-n for the upper half */
		int half = (SIGRTMAX - SIGRTMIN) / 2, n;
		char *p;
		if (sig - SIGRTMIN <= half) { p = stpcpy(s, "RTMIN"); n = sig - SIGRTMIN; if (n) *p++ = '+'; }
		else { p = stpcpy(s, "RTMAX"); n = SIGRTMAX - sig; if (n) *p++ = '-'; }
		if (n >= 10) *p++ = (char)('0' + n / 10);
		if (n) *p++ = (char)('0' + n % 10);
		*p = 0;
		return 0;
	}
	return -1;
}

static int num(const char *s, int *v)
{
	int n = 0;
	if (!*s) return -1;
	for (; *s; s++) {
		if ((unsigned)*s - '0' >= 10 || n > 1000) return -1;
		n = n * 10 + (*s - '0');
	}
	*v = n;
	return 0;
}

int str2sig(const char *__RESTRICT s, int *__RESTRICT sig)
{
	int i, n;
	if ((unsigned)*s - '0' < 10) {
		if (num(s, &n) || n < 1 || n >= _NSIG || (unsigned)n - 32 < 3) return -1;
		*sig = n;
		return 0;
	}
	for (i = 1; i < 32; i++)
		if (!strcmp(s, names[i])) { *sig = i; return 0; }
	if (!strcmp(s, "IOT")) { *sig = SIGABRT; return 0; }
	if (!strcmp(s, "IO")) { *sig = SIGIO; return 0; }
	if (!strncmp(s, "RTMIN", 5)) {
		if (!s[5]) { *sig = SIGRTMIN; return 0; }
		if (s[5] == '+' && !num(s + 6, &n) && SIGRTMIN + n <= SIGRTMAX) { *sig = SIGRTMIN + n; return 0; }
	}
	if (!strncmp(s, "RTMAX", 5)) {
		if (!s[5]) { *sig = SIGRTMAX; return 0; }
		if (s[5] == '-' && !num(s + 6, &n) && SIGRTMAX - n >= SIGRTMIN) { *sig = SIGRTMAX - n; return 0; }
	}
	return -1;
}

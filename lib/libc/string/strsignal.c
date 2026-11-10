#include <string.h>

static const char *const names[] = {

	[1] = "Hangup", [2] = "Interrupt", [3] = "Quit",
	[4] = "Illegal instruction", [5] = "Trace/breakpoint trap",
	[6] = "Aborted", [7] = "Bus error", [8] = "Floating point exception",
	[9] = "Killed", [10] = "User defined signal 1", [11] = "Segmentation fault",
	[12] = "User defined signal 2", [13] = "Broken pipe", [14] = "Alarm clock",
	[15] = "Terminated", [16] = "Stack fault", [17] = "Child exited",
	[18] = "Continued", [19] = "Stopped (signal)", [20] = "Stopped",
	[21] = "Stopped (tty input)", [22] = "Stopped (tty output)",
	[23] = "Urgent I/O condition", [24] = "CPU time limit exceeded",
	[25] = "File size limit exceeded", [26] = "Virtual timer expired",
	[27] = "Profiling timer expired", [28] = "Window changed",
	[29] = "I/O possible", [30] = "Power failure", [31] = "Bad system call",
};

char *strsignal(int sig)
{
	static char buf[32];
	char *p;
	unsigned u;
	if (sig > 0 && sig < 32) return (char *)names[sig];
	if (sig >= 35 && sig <= 64) {
		/* 32-34 are reserved for the library; 35 is SIGRTMIN */
		int n = sig - 35;
		p = stpcpy(buf, "Real-time signal ");
		if (n >= 10) *p++ = (char)('0' + n / 10);
		*p++ = (char)('0' + n % 10);
		*p = 0;
		return buf;
	}
	/* as glibc: "Unknown signal N" (also for the library's own 32-34) */
	p = stpcpy(buf, "Unknown signal ");
	u = sig < 0 ? 0u - (unsigned)sig : (unsigned)sig;
	if (sig < 0) *p++ = '-';
	{
		char t[12];
		int k = 0;
		do t[k++] = (char)('0' + u % 10); while (u /= 10);
		while (k) *p++ = t[--k];
	}
	*p = 0;
	return buf;
}

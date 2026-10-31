/* getopt(), as POSIX specifies it: scanning stops at the first operand
 * (no permutation), and "--" ends the options. */
#include <unistd.h>
#include <string.h>
#include "syscall.h"

char *optarg;
int optind = 1, opterr = 1, optopt;
static int pos;      /* position within the current argument; 0 = start */
static int last_optind;

static void msg(const char *prog, const char *text, int c)
{
	char ch = (char)c;
	size_t n = strlen(prog);
	__syscall3(SYS_write, 2, prog, n);
	__syscall3(SYS_write, 2, text, strlen(text));
	__syscall3(SYS_write, 2, &ch, 1);
	__syscall3(SYS_write, 2, "\n", 1);
}

int getopt(int argc, char *const argv[], const char *opts)
{
	const char *arg, *p;
	int c;

	/* an application may reset optind to start over */
	if (optind == 0 || optind != last_optind) pos = 0;
	if (optind == 0) optind = 1;
	optarg = 0;
	if (!pos) {
		if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1]) {
			last_optind = optind;
			return -1;
		}
		if (!strcmp(argv[optind], "--")) { optind++; last_optind = optind; return -1; }
		pos = 1;
	}
	arg = argv[optind];
	c = (unsigned char)arg[pos++];
	p = c == ':' ? 0 : strchr(opts + (*opts == ':'), c);
	if (!p) {
		optopt = c;
		if (!arg[pos]) { optind++; pos = 0; }
		if (opterr && *opts != ':') msg(argv[0], ": illegal option -- ", c);
		last_optind = optind;
		return '?';
	}
	if (p[1] == ':') {
		if (arg[pos]) optarg = (char *)arg + pos;
		else if (optind + 1 < argc) optarg = argv[++optind];
		else {
			optopt = c;
			optind++; pos = 0; last_optind = optind;
			if (*opts == ':') return ':';
			if (opterr) msg(argv[0], ": option requires an argument -- ", c);
			return '?';
		}
		optind++; pos = 0;
	} else if (!arg[pos]) {
		optind++; pos = 0;
	}
	last_optind = optind;
	return c;
}

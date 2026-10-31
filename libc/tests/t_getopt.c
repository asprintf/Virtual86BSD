/* getopt() against glibc's POSIX getopt (no permutation). */
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <string.h>
#include "out.h"

static const char *cases[][8] = {
	{ "p", "-a", "-b", "x", 0 },
	{ "p", "-ab", "-c", "arg", "rest", 0 },
	{ "p", "-carg", "-a", 0 },
	{ "p", "-c", 0 },
	{ "p", "-z", "-a", 0 },
	{ "p", "--", "-a", 0 },
	{ "p", "-", "-a", 0 },
	{ "p", "x", "-a", 0 },
	{ "p", "-a", "--", 0 },
	{ "p", "-aaab", 0 },
	{ "p", "-:", "-a", 0 },
	{ "p", "-ac", 0 },
	{ "p", "-c", "--", "-a", 0 },
	{ "p", 0 },
};
static const char *optstrs[] = { "abc:", ":abc:", "ab", "c:a" };

int main(void)
{
	size_t i, j;
	opterr = 0;
	for (j = 0; j < sizeof optstrs / sizeof *optstrs; j++)
		for (i = 0; i < sizeof cases / sizeof *cases; i++) {
			char *argv[8];
			int argc = 0, c;
			while (cases[i][argc]) { argv[argc] = (char *)cases[i][argc]; argc++; }
			argv[argc] = 0;
			optind = 1;
			os(optstrs[j]); oc(' '); oi((long long)i); oc(':');
			while ((c = getopt(argc, argv, optstrs[j])) != -1) {
				oc(' '); oc((char)c);
				if (c == '?' || c == ':') { oc('['); oc((char)optopt); oc(']'); }
				if (optarg) { oc('='); os(optarg); }
				oc('@'); oi(optind);
			}
			os(" end@"); oi(optind); onl();
		}
	oflush();
	return 0;
}

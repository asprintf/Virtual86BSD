/* Differential: fmtmsg() to standard error, with MSGVERB settings.
 * Each call is made in a child process, because glibc reads MSGVERB
 * once per process. */
#define _XOPEN_SOURCE 800
#include <fmtmsg.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "out.h"

static void run(const char *verb, long cls, const char *label, int sev,
                const char *text, const char *action, const char *tag)
{
	int p[2], st;
	char buf[4096];
	ssize_t n;
	pid_t pid;
	if (pipe(p))
		return;
	if (!(pid = fork())) {
		char rc[8];
		int r;
		close(p[0]);
		dup2(p[1], 2);
		if (verb) setenv("MSGVERB", verb, 1);
		else unsetenv("MSGVERB");
		r = fmtmsg(cls, label, sev, text, action, tag);
		rc[0] = '['; rc[1] = r < 0 ? '-' : '+'; rc[2] = (char)('0' + (r < 0 ? -r : r)); rc[3] = ']'; rc[4] = '\n';
		write(2, rc, 5);
		_exit(0);
	}
	close(p[1]);
	while ((n = read(p[0], buf, sizeof buf)) > 0)
		for (ssize_t i = 0; i < n; i++) oc(buf[i]);
	close(p[0]);
	waitpid(pid, &st, 0);
}

int main(void)
{
	static const char *verbs[] = {
		0, "", "label", "severity", "text", "action", "tag", "severity:text:action",
		"tag:label", "label:severity:text:action:tag", "bogus", "label:bogus", "text:",
		":text", "label::tag", "text:text", "Text", "labelx",
	};
	static const char *labels[] = { "XSI:cat", 0, "nocolon", "0123456789:x", "01234567890:x",
	                                "a:01234567890123", "a:012345678901234", ":", "UX:sort" };
	unsigned i, j;
	for (i = 0; i < sizeof verbs / sizeof *verbs; i++) {
		os("MSGVERB="); os(verbs[i] ? verbs[i] : "(unset)"); onl();
		run(verbs[i], MM_PRINT | MM_SOFT | MM_UTIL, "XSI:cat", MM_ERROR, "illegal option",
		    "refer to cat in user's reference manual", "XSI:cat:001");
		run(verbs[i], MM_PRINT, 0, MM_INFO, "only text", 0, 0);
		run(verbs[i], MM_PRINT, "UX:x", MM_NOSEV, 0, "act", 0);
		run(verbs[i], MM_PRINT, "UX:x", MM_HALT, 0, 0, "tag");
	}
	for (i = 0; i < sizeof labels / sizeof *labels; i++)
		for (j = 0; j < 7; j++)
			run(0, MM_PRINT, labels[i], (int)j - 1, j & 1 ? "text" : 0, j & 2 ? "action" : 0, j & 4 ? 0 : "tag");
	run(0, MM_PRINT, 0, 0, 0, 0, 0);
	run(0, MM_SOFT, "XSI:cat", MM_ERROR, "not printed", 0, 0);
	run(0, MM_PRINT, "XSI:cat", MM_WARNING, "", "", "");
	run(0, MM_PRINT, "XSI:cat", 99, "bad severity", 0, 0);
	oflush();
	return 0;
}

/* fmtmsg: the message is assembled first and written with one call.
 * MSGVERB selects the components for standard error (all of them if it
 * is unset, empty or holds an unknown keyword); the console gets every
 * component.  Like glibc: a label must be "x:y" with at most 10 and 14
 * bytes, an unknown severity fails (MM_NOTOK), and a tag follows the
 * action after two spaces. */
#include <fmtmsg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const sevname[] = { "", "HALT", "ERROR", "WARNING", "INFO" };
static const char *const keyword[] = { "label", "severity", "text", "action", "tag" };

static int verbosity(void)
{
	const char *s = getenv("MSGVERB");
	int sel = 0, i;
	size_t n;
	if (!s || !*s)
		return 31;
	while (*s) {
		for (i = 0; i < 5; i++) {
			n = strlen(keyword[i]);
			if (!strncmp(s, keyword[i], n) && (s[n] == ':' || !s[n]))
				break;
		}
		if (i == 5)
			return 31;
		sel |= 1 << i;
		s += n;
		if (*s == ':')
			s++;
	}
	return sel;
}

/* the message with the components in sel; malloc'd */
static char *compose(int sel, const char *label, const char *sev, const char *text,
                     const char *action, const char *tag)
{
	const char *part[10];
	int l = sel & 1 && label, s = sel & 2 && sev, t = sel & 4 && text;
	int a = sel & 8 && action, g = sel & 16 && tag, i;
	size_t len = 2;
	char *m, *p;
	part[0] = l ? label : "";
	part[1] = l && (s | t | a | g) ? ": " : "";
	part[2] = s ? sev : "";
	part[3] = s && (t | a | g) ? ": " : "";
	part[4] = t ? text : "";
	part[5] = t && (a | g) ? "\n" : "";
	part[6] = a ? "TO FIX: " : "";
	part[7] = a ? action : "";
	part[8] = a && g ? "  " : "";
	part[9] = g ? tag : "";
	for (i = 0; i < 10; i++)
		len += strlen(part[i]);
	if (!(m = p = malloc(len)))
		return 0;
	for (i = 0; i < 10; i++)
		p = stpcpy(p, part[i]);
	strcpy(p, "\n");
	return m;
}

int fmtmsg(long class, const char *label, int severity, const char *text,
           const char *action, const char *tag)
{
	const char *sev;
	const char *colon;
	char *m;
	int r = MM_OK, fd;
	size_t n;

	if (label) {
		if (!(colon = strchr(label, ':')) || colon - label > 10 || strlen(colon + 1) > 14)
			return MM_NOTOK;
	}
	if (severity < 0 || severity > MM_INFO)
		return MM_NOTOK;
	sev = severity ? sevname[severity] : 0;

	if (class & MM_PRINT) {
		m = compose(verbosity(), label, sev, text, action, tag);
		n = m ? strlen(m) : 0;
		if (!m || fwrite(m, 1, n, stderr) != n || fflush(stderr))
			r |= MM_NOMSG;
		free(m);
	}
	if (class & MM_CONSOLE) {
		m = compose(31, label, sev, text, action, tag);
		fd = m ? open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC) : -1;
		n = m ? strlen(m) : 0;
		if (fd < 0 || write(fd, m, n) != (ssize_t)n)
			r |= MM_NOCON;
		if (fd >= 0)
			close(fd);
		free(m);
	}
	return r == (MM_NOMSG | MM_NOCON) ? MM_NOTOK : r;
}

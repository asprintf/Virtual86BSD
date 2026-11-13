/* terminfo without curses: setupterm, tigetflag, tigetnum, tigetstr,
 * tparm, tputs, putp, restartterm, vidputs, the termcap functions
 * OPS: w: */
#include "ct.h"
#include <term.h>

static char obuf[256];
static size_t olen;

static int
cap(int c)
{
	if (olen < sizeof obuf - 1)
		obuf[olen++] = (char)c;
	obuf[olen] = '\0';
	return c;
}

/* s, its escape characters as \E */
static const char *
esc(const char *s)
{
	static char b[256];
	size_t i = 0;

	if (s == NULL)
		return "(null)";
	if (s == (char *)-1)
		return "(-1)";
	for (; *s != '\0' && i < sizeof b - 3; s++) {
		if (*s == 033) {
			b[i++] = '\\';
			b[i++] = 'E';
		} else
			b[i++] = *s;
	}
	b[i] = '\0';
	return b;
}

int
main(void)
{
	int err = 99, r;
	char area[128], *ap = area, *cm;

	r = setupterm("nosuchterminal", 1, &err);
	lg("setupterm nosuchterminal %s errret %d", rv(r), err);
	r = setupterm(NULL, 1, &err);
	lg("setupterm $TERM %s errret %d", rv(r), err);
	lg("tigetnum cols %d lines %d", tigetnum("cols"), tigetnum("lines"));
	lg("tigetnum of a string %d, of nothing %d", tigetnum("cup"), tigetnum("xyzzy"));
	lg("tigetflag am %d bw %d, of a number %d", tigetflag("am"), tigetflag("bw"), tigetflag("cols"));
	lg("tigetstr cup %s", esc(tigetstr("cup")));
	lg("tigetstr of a number %s, of nothing %s", esc(tigetstr("cols")), esc(tigetstr("xyzzy")));
	lg("tparm cup 2 3 %s", esc(tparm(tigetstr("cup"), 2, 3, 0, 0, 0, 0, 0, 0, 0)));
	lg("tparm setaf 4 %s", esc(tparm(tigetstr("setaf"), 4, 0, 0, 0, 0, 0, 0, 0, 0)));
	olen = 0;
	r = tputs(tigetstr("clear"), 1, cap);
	lg("tputs clear %s %s", rv(r), esc(obuf));
	olen = 0;
	r = vidputs(A_BOLD | A_UNDERLINE, cap);
	lg("vidputs bold underline %s %s", rv(r), esc(obuf));
	olen = 0;
	r = vid_puts(WA_REVERSE, 0, NULL, cap);
	lg("vid_puts reverse %s %s", rv(r), esc(obuf));
	lg("vid_puts with opts %s", rv(vid_puts(WA_REVERSE, 0, obuf, cap)));
	lg("tgetent %d", tgetent(NULL, "v86test"));
	lg("tgetnum co %d li %d", tgetnum("co"), tgetnum("li"));
	lg("tgetflag am %d", tgetflag("am"));
	cm = tgetstr("cm", &ap);
	lg("tgetstr cm %s, in the area %d", esc(cm), cm == area);
	lg("tgoto col 3 line 2 %s", esc(tgoto(cm, 3, 2)));
	err = 99;
	r = restartterm("v86test", 1, &err);
	lg("restartterm %s errret %d cols %d", rv(r), err, tigetnum("cols"));
	r = putp(tigetstr("clear"));
	lg("putp %s", rv(r));
	fflush(stdout);
	printf("terminfo");
	fflush(stdout);
	lg("del_curterm %s", rv(del_curterm(cur_term)));
	return 0;
}

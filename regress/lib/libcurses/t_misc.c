/* unctrl, keyname, erasechar, killchar, has_ic, has_il, beep, flash,
 * curs_set, the version of X/Open Curses
 * OPS: w: */
#include "ct.h"
#include <unctrl.h>

int
main(void)
{
	initscr();
	lg("_XOPEN_CURSES %d", _XOPEN_CURSES + 0 > 0);
	lg("unctrl a [%s] ^A [%s] DEL [%s] space [%s]", unctrl('a'), unctrl(1), unctrl(0177), unctrl(' '));
	lg("unctrl of bold a [%s]", unctrl('a' | A_BOLD));
	/* (each in its own call: the string may be a static one) */
	lg("keyname a [%s]", keyname('a'));
	lg("keyname ^A [%s]", keyname(1));
	lg("keyname KEY_UP [%s]", keyname(KEY_UP));
	lg("keyname KEY_F(1) [%s]", keyname(KEY_F(1)));
	lg("erasechar %d killchar %d", erasechar(), killchar());
	lg("has_ic %d has_il %d", has_ic(), has_il());
	lg("beep %s", rv(beep()));
	lg("flash %s", rv(flash()));
	lg("curs_set %d", curs_set(1));
	mvaddstr(0, 0, "misc");
	refresh();
	endwin();
	return 0;
}

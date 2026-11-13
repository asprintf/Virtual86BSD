/* the wide-character functions with ASCII: addwstr, add_wch, setcchar,
 * getcchar, in_wch, inwstr, get_wch, wunctrl
 * OPS: e:ready s:xy\eOA w: */
#include "ct.h"
#include <wchar.h>

int
main(void)
{
	cchar_t cc, rc;
	wchar_t ws[8] = L"w", out[32];
	attr_t a;
	short p;
	wint_t wc;
	int r;

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	lg("addwstr %s", rv(mvaddwstr(0, 0, L"wide")));
	lg("setcchar %s", rv(setcchar(&cc, ws, WA_BOLD, 0, NULL)));
	lg("add_wch %s", rv(mvadd_wch(1, 0, &cc)));
	memset(&rc, 0x5a, sizeof rc);	/* (in_wch() must not leave any of it) */
	lg("in_wch %s", rv(mvin_wch(1, 0, &rc)));
	ws[0] = 0;
	r = getcchar(&rc, ws, &a, &p, NULL);
	lg("getcchar %s [%lc] bold %d pair %d", rv(r), (wint_t)ws[0], (a & WA_BOLD) != 0, p);
	lg("getcchar length %d", getcchar(&rc, NULL, &a, &p, NULL));
	r = mvinwstr(0, 0, out);
	lg("inwstr %s [%ls]", rv(r), out);
	mvaddstr(3, 0, "ready");
	refresh();
	r = get_wch(&wc);
	lg("get_wch %s %lc", rv(r), wc);
	r = get_wch(&wc);
	lg("get_wch %s %lc", rv(r), wc);
	r = get_wch(&wc);
	lg("get_wch %s key %d", r == KEY_CODE_YES ? "KEY_CODE_YES" : rv(r), wc == KEY_UP);
	ws[0] = 1;
	ws[1] = 0;
	setcchar(&cc, ws, 0, 0, NULL);
	out[0] = 0;
	lg("wunctrl ^A [%ls]", wunctrl(&cc));
	refresh();
	endwin();
	return 0;
}

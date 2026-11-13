/* getnstr and getn_wstr with echo: the erase and kill characters, the limit
 * OPS: e:name? s:abx\x7fc\r e:two? s:wrong\x15right\r e:three? s:toolongtext\r e:four? s:wide\x15ok\r w: */
#include "ct.h"

int
main(void)
{
	char buf[64];
	wint_t wbuf[64];
	int r, i;

	initscr();
	cbreak();
	echo();
	mvaddstr(0, 0, "name? ");
	refresh();
	r = getnstr(buf, 20);
	lg("getnstr %s [%s]", rv(r), buf);
	mvaddstr(2, 0, "two? ");
	refresh();
	r = getnstr(buf, 20);
	lg("getnstr %s [%s]", rv(r), buf);
	mvaddstr(4, 0, "three? ");
	refresh();
	r = getnstr(buf, 5);
	lg("getnstr 5 %s [%s]", rv(r), buf);
	mvaddstr(6, 0, "four? ");
	refresh();
	r = getn_wstr(wbuf, 20);
	for (i = 0; wbuf[i] != 0; i++)
		buf[i] = (char)wbuf[i];
	buf[i] = '\0';
	lg("getn_wstr %s [%s]", rv(r), buf);
	endwin();
	return 0;
}

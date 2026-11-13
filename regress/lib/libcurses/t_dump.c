/* scr_dump and scr_restore, putwin and getwin
 * OPS: w: */
#include "ct.h"

int
main(void)
{
	char path[256];
	const char *log = getenv("CT_LOG");
	FILE *fp;
	WINDOW *w, *g;

	snprintf(path, sizeof path, "%s.dump", log != NULL ? log : "/tmp/t_dump");
	initscr();
	mvaddstr(0, 0, "dumped");
	attron(A_BOLD);
	mvaddstr(1, 2, "bold");
	attroff(A_BOLD);
	refresh();
	lg("scr_dump %s", rv(scr_dump(path)));
	clear();
	mvaddstr(5, 0, "cleared");
	refresh();
	lg("scr_restore %s", rv(scr_restore(path)));
	doupdate();
	lg("scr_restore of nothing %s", rv(scr_restore("/nonexistent/x")));
	w = newwin(2, 6, 4, 10);
	mvwaddstr(w, 1, 1, "pw");
	fp = fopen(path, "w");
	lg("putwin %s", rv(putwin(w, fp)));
	fclose(fp);
	fp = fopen(path, "r");
	g = getwin(fp);
	fclose(fp);
	lg("getwin %s", g != NULL ? "ok" : "NULL");
	if (g != NULL) {
		mvwin(g, 6, 12);
		wrefresh(g);
	}
	remove(path);
	endwin();
	return 0;
}

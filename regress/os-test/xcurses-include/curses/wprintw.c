/*[XCurses7]*/
#include <curses.h>
#ifdef wprintw
#undef wprintw
#endif
int (*foo)(WINDOW *, const char *, ...) = wprintw;
int main(void) { return 0; }

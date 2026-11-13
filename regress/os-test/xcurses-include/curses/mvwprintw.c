/*[XCurses7]*/
#include <curses.h>
#ifdef mvwprintw
#undef mvwprintw
#endif
int (*foo)(WINDOW *, int, int, const char *, ...) = mvwprintw;
int main(void) { return 0; }

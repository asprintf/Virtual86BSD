/*[XCurses7]*/
#include <curses.h>
#ifdef mvprintw
#undef mvprintw
#endif
int (*foo)(int, int, const char *, ...) = mvprintw;
int main(void) { return 0; }

/*[XCurses7]*/
#include <curses.h>
#ifdef printw
#undef printw
#endif
int (*foo)(const char *, ...) = printw;
int main(void) { return 0; }

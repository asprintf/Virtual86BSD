/*[XCurses7]*/
#include <curses.h>
#ifdef vw_printw
#undef vw_printw
#endif
int (*foo)(WINDOW *, const char *, va_list) = vw_printw;
int main(void) { return 0; }

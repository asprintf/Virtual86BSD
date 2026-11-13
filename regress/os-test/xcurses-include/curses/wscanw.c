/*[XCurses7]*/
#include <curses.h>
#ifdef wscanw
#undef wscanw
#endif
int (*foo)(WINDOW *, const char *, ...) = wscanw;
int main(void) { return 0; }

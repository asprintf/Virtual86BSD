/*[XCurses7]*/
#include <curses.h>
#ifdef mvwscanw
#undef mvwscanw
#endif
int (*foo)(WINDOW *, int, int, const char *, ...) = mvwscanw;
int main(void) { return 0; }

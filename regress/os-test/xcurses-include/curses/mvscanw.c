/*[XCurses7]*/
#include <curses.h>
#ifdef mvscanw
#undef mvscanw
#endif
int (*foo)(int, int, const char *, ...) = mvscanw;
int main(void) { return 0; }

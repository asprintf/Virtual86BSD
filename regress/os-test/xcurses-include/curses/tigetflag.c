/*[XCurses7]*/
#include <curses.h>
#ifdef tigetflag
#undef tigetflag
#endif
int (*foo)(const char *) = tigetflag;
int main(void) { return 0; }

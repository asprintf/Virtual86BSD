/*[XCurses7]*/
#include <curses.h>
#ifdef tigetnum
#undef tigetnum
#endif
int (*foo)(const char *) = tigetnum;
int main(void) { return 0; }

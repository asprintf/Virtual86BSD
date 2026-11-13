/*[XCurses7]*/
#include <curses.h>
#ifdef scanw
#undef scanw
#endif
int (*foo)(const char *, ...) = scanw;
int main(void) { return 0; }

/*[XCurses7]*/
#include <curses.h>
#ifdef newterm
#undef newterm
#endif
SCREEN *(*foo)(const char *, FILE *, FILE *) = newterm;
int main(void) { return 0; }

/*[XCurses7]*/
#include <curses.h>
#ifdef vw_scanw
#undef vw_scanw
#endif
int (*foo)(WINDOW *, const char *, va_list) = vw_scanw;
int main(void) { return 0; }

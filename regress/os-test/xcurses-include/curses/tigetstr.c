/*[XCurses7]*/
#include <curses.h>
#ifdef tigetstr
#undef tigetstr
#endif
char *(*foo)(const char *) = tigetstr;
int main(void) { return 0; }

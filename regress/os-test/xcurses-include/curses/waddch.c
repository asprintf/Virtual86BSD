/*[SUSv2]*/
#include <curses.h>
#ifdef waddch
#undef waddch
#endif
int (*foo)(WINDOW *, const chtype) = waddch;
int main(void) { return 0; }

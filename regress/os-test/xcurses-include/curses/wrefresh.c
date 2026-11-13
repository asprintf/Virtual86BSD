/*[SUSv2]*/
#include <curses.h>
#ifdef wrefresh
#undef wrefresh
#endif
int (*foo)(WINDOW *) = wrefresh;
int main(void) { return 0; }

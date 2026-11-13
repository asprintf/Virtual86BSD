/*[SUSv2]*/
#include <curses.h>
#ifdef wattron
#undef wattron
#endif
int (*foo)(WINDOW *, int) = wattron;
int main(void) { return 0; }

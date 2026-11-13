/*[SUSv2]*/
#include <curses.h>
#ifdef wdeleteln
#undef wdeleteln
#endif
int (*foo)(WINDOW *) = wdeleteln;
int main(void) { return 0; }

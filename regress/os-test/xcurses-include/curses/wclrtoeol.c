/*[SUSv2]*/
#include <curses.h>
#ifdef wclrtoeol
#undef wclrtoeol
#endif
int (*foo)(WINDOW *) = wclrtoeol;
int main(void) { return 0; }

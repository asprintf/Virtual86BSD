/*[SUSv2]*/
#include <curses.h>
#ifdef initscr
#undef initscr
#endif
WINDOW *(*foo)(void) = initscr;
int main(void) { return 0; }

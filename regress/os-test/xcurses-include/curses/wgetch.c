/*[SUSv2]*/
#include <curses.h>
#ifdef wgetch
#undef wgetch
#endif
int (*foo)(WINDOW *) = wgetch;
int main(void) { return 0; }

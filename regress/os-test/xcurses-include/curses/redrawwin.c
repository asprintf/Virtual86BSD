/*[SUSv2]*/
#include <curses.h>
#ifdef redrawwin
#undef redrawwin
#endif
int (*foo)(WINDOW *) = redrawwin;
int main(void) { return 0; }

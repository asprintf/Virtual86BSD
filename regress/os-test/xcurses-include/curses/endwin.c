/*[SUSv2]*/
#include <curses.h>
#ifdef endwin
#undef endwin
#endif
int (*foo)(void) = endwin;
int main(void) { return 0; }

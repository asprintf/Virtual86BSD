/*[SUSv2]*/
#include <curses.h>
#ifdef refresh
#undef refresh
#endif
int (*foo)(void) = refresh;
int main(void) { return 0; }

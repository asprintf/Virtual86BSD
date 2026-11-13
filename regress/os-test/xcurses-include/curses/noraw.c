/*[SUSv2]*/
#include <curses.h>
#ifdef noraw
#undef noraw
#endif
int (*foo)(void) = noraw;
int main(void) { return 0; }

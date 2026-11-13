/*[SUSv2]*/
#include <curses.h>
#ifdef noecho
#undef noecho
#endif
int (*foo)(void) = noecho;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef nonl
#undef nonl
#endif
int (*foo)(void) = nonl;
int main(void) { return 0; }

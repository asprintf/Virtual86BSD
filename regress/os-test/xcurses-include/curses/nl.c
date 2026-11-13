/*[SUSv2]*/
#include <curses.h>
#ifdef nl
#undef nl
#endif
int (*foo)(void) = nl;
int main(void) { return 0; }

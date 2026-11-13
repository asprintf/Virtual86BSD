/*[SUSv2]*/
#include <curses.h>
#ifdef clrtoeol
#undef clrtoeol
#endif
int (*foo)(void) = clrtoeol;
int main(void) { return 0; }

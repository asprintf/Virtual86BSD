/*[SUSv2]*/
#include <curses.h>
#ifdef doupdate
#undef doupdate
#endif
int (*foo)(void) = doupdate;
int main(void) { return 0; }

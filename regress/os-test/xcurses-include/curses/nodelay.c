/*[SUSv2]*/
#include <curses.h>
#ifdef nodelay
#undef nodelay
#endif
int (*foo)(WINDOW *, bool) = nodelay;
int main(void) { return 0; }

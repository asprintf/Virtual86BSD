/*[SUSv2]*/
#include <curses.h>
#ifdef notimeout
#undef notimeout
#endif
int (*foo)(WINDOW *, bool) = notimeout;
int main(void) { return 0; }

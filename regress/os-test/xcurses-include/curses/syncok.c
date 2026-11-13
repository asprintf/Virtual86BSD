/*[SUSv2]*/
#include <curses.h>
#ifdef syncok
#undef syncok
#endif
int (*foo)(WINDOW *, bool) = syncok;
int main(void) { return 0; }

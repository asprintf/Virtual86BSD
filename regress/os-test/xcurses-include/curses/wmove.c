/*[SUSv2]*/
#include <curses.h>
#ifdef wmove
#undef wmove
#endif
int (*foo)(WINDOW *, int, int) = wmove;
int main(void) { return 0; }

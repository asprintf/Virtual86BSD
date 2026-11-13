/*[SUSv2]*/
#include <curses.h>
#ifdef wscrl
#undef wscrl
#endif
int (*foo)(WINDOW *, int) = wscrl;
int main(void) { return 0; }

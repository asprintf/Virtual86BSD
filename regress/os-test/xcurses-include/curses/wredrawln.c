/*[SUSv2]*/
#include <curses.h>
#ifdef wredrawln
#undef wredrawln
#endif
int (*foo)(WINDOW *, int, int) = wredrawln;
int main(void) { return 0; }

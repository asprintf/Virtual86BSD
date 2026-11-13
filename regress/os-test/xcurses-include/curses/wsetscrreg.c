/*[SUSv2]*/
#include <curses.h>
#ifdef wsetscrreg
#undef wsetscrreg
#endif
int (*foo)(WINDOW *, int, int) = wsetscrreg;
int main(void) { return 0; }

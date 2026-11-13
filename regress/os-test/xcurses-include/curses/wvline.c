/*[SUSv2]*/
#include <curses.h>
#ifdef wvline
#undef wvline
#endif
int (*foo)(WINDOW *, chtype, int) = wvline;
int main(void) { return 0; }

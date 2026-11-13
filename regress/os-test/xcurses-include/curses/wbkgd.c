/*[SUSv2]*/
#include <curses.h>
#ifdef wbkgd
#undef wbkgd
#endif
int (*foo)(WINDOW *, chtype) = wbkgd;
int main(void) { return 0; }

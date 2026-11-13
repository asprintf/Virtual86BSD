/*[SUSv2]*/
#include <curses.h>
#ifdef winchnstr
#undef winchnstr
#endif
int (*foo)(WINDOW *, chtype *, int) = winchnstr;
int main(void) { return 0; }

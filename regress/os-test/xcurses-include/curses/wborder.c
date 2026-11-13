/*[SUSv2]*/
#include <curses.h>
#ifdef wborder
#undef wborder
#endif
int (*foo)(WINDOW *, chtype, chtype, chtype, chtype, chtype, chtype, chtype, chtype) = wborder;
int main(void) { return 0; }

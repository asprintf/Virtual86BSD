/*[SUSv2]*/
#include <curses.h>
#ifdef whline
#undef whline
#endif
int (*foo)(WINDOW *, chtype, int) = whline;
int main(void) { return 0; }

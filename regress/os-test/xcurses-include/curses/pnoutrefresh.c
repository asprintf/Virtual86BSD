/*[SUSv2]*/
#include <curses.h>
#ifdef pnoutrefresh
#undef pnoutrefresh
#endif
int (*foo)(WINDOW *, int, int, int, int, int, int) = pnoutrefresh;
int main(void) { return 0; }

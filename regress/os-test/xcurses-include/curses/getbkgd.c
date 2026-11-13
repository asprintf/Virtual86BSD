/*[SUSv2]*/
#include <curses.h>
#ifdef getbkgd
#undef getbkgd
#endif
chtype (*foo)(WINDOW *) = getbkgd;
int main(void) { return 0; }

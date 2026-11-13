/*[SUSv2]*/
#include <curses.h>
#ifdef winch
#undef winch
#endif
chtype (*foo)(WINDOW *) = winch;
int main(void) { return 0; }

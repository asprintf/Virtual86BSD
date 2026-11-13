/*[SUSv2]*/
#include <curses.h>
#ifdef inch
#undef inch
#endif
chtype (*foo)(void) = inch;
int main(void) { return 0; }

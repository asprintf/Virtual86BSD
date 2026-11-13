/*[SUSv2]*/
#include <curses.h>
#ifdef mvinch
#undef mvinch
#endif
chtype (*foo)(int, int) = mvinch;
int main(void) { return 0; }

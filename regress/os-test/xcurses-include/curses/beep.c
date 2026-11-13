/*[SUSv2]*/
#include <curses.h>
#ifdef beep
#undef beep
#endif
int (*foo)(void) = beep;
int main(void) { return 0; }

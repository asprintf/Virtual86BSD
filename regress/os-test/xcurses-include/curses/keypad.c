/*[SUSv2]*/
#include <curses.h>
#ifdef keypad
#undef keypad
#endif
int (*foo)(WINDOW *, bool) = keypad;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wclrtobot
#undef wclrtobot
#endif
int (*foo)(WINDOW *) = wclrtobot;
int main(void) { return 0; }

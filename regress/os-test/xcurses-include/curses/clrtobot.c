/*[SUSv2]*/
#include <curses.h>
#ifdef clrtobot
#undef clrtobot
#endif
int (*foo)(void) = clrtobot;
int main(void) { return 0; }

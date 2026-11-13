/*[SUSv2]*/
#include <curses.h>
#ifdef attron
#undef attron
#endif
int (*foo)(int) = attron;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef clear
#undef clear
#endif
int (*foo)(void) = clear;
int main(void) { return 0; }

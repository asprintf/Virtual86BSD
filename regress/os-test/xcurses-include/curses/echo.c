/*[SUSv2]*/
#include <curses.h>
#ifdef echo
#undef echo
#endif
int (*foo)(void) = echo;
int main(void) { return 0; }

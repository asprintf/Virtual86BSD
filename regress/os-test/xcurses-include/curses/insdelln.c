/*[SUSv2]*/
#include <curses.h>
#ifdef insdelln
#undef insdelln
#endif
int (*foo)(int) = insdelln;
int main(void) { return 0; }

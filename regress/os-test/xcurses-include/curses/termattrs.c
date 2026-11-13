/*[SUSv2]*/
#include <curses.h>
#ifdef termattrs
#undef termattrs
#endif
chtype (*foo)(void) = termattrs;
int main(void) { return 0; }

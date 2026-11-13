/*[SUSv2]*/
#include <curses.h>
#ifdef vline
#undef vline
#endif
int (*foo)(chtype, int) = vline;
int main(void) { return 0; }

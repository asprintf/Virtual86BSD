/*[SUSv2]*/
#include <curses.h>
#ifdef hline
#undef hline
#endif
int (*foo)(chtype, int) = hline;
int main(void) { return 0; }

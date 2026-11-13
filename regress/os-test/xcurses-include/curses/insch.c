/*[SUSv2]*/
#include <curses.h>
#ifdef insch
#undef insch
#endif
int (*foo)(chtype) = insch;
int main(void) { return 0; }

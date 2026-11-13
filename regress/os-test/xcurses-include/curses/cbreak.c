/*[SUSv2]*/
#include <curses.h>
#ifdef cbreak
#undef cbreak
#endif
int (*foo)(void) = cbreak;
int main(void) { return 0; }

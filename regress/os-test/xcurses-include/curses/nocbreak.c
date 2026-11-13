/*[SUSv2]*/
#include <curses.h>
#ifdef nocbreak
#undef nocbreak
#endif
int (*foo)(void) = nocbreak;
int main(void) { return 0; }

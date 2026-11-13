/*[SUSv2]*/
#include <curses.h>
#ifdef killchar
#undef killchar
#endif
char (*foo)(void) = killchar;
int main(void) { return 0; }

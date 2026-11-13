/*[SUSv2]*/
#include <curses.h>
#ifdef erasechar
#undef erasechar
#endif
char (*foo)(void) = erasechar;
int main(void) { return 0; }

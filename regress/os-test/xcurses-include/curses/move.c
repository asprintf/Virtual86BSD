/*[SUSv2]*/
#include <curses.h>
#ifdef move
#undef move
#endif
int (*foo)(int, int) = move;
int main(void) { return 0; }

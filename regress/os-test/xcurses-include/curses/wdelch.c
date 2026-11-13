/*[SUSv2]*/
#include <curses.h>
#ifdef wdelch
#undef wdelch
#endif
int (*foo)(WINDOW *) = wdelch;
int main(void) { return 0; }

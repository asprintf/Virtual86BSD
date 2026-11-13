/*[SUSv2]*/
#include <curses.h>
#ifdef winsertln
#undef winsertln
#endif
int (*foo)(WINDOW *) = winsertln;
int main(void) { return 0; }

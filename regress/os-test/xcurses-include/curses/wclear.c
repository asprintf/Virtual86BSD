/*[SUSv2]*/
#include <curses.h>
#ifdef wclear
#undef wclear
#endif
int (*foo)(WINDOW *) = wclear;
int main(void) { return 0; }

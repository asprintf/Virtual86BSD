/*[SUSv2]*/
#include <curses.h>
#ifdef scroll
#undef scroll
#endif
int (*foo)(WINDOW *) = scroll;
int main(void) { return 0; }

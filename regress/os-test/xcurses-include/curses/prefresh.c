/*[SUSv2]*/
#include <curses.h>
#ifdef prefresh
#undef prefresh
#endif
int (*foo)(WINDOW *, int, int, int, int, int, int) = prefresh;
int main(void) { return 0; }

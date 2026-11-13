/*[SUSv2]*/
#include <curses.h>
#ifdef subpad
#undef subpad
#endif
WINDOW *(*foo)(WINDOW *, int, int, int, int) = subpad;
int main(void) { return 0; }

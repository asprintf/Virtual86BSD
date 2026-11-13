/*[SUSv2]*/
#include <curses.h>
#ifdef subwin
#undef subwin
#endif
WINDOW *(*foo)(WINDOW *, int, int, int, int) = subwin;
int main(void) { return 0; }

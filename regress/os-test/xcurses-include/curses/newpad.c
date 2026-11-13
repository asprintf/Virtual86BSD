/*[SUSv2]*/
#include <curses.h>
#ifdef newpad
#undef newpad
#endif
WINDOW *(*foo)(int, int) = newpad;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef newwin
#undef newwin
#endif
WINDOW *(*foo)(int, int, int, int) = newwin;
int main(void) { return 0; }

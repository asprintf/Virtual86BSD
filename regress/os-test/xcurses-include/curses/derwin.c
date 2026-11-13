/*[SUSv2]*/
#include <curses.h>
#ifdef derwin
#undef derwin
#endif
WINDOW *(*foo)(WINDOW *, int, int, int, int) = derwin;
int main(void) { return 0; }

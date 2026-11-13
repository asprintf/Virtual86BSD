/*[SUSv2]*/
#include <curses.h>
#ifdef dupwin
#undef dupwin
#endif
WINDOW *(*foo)(WINDOW *) = dupwin;
int main(void) { return 0; }

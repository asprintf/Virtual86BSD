/*[SUSv2]*/
#include <curses.h>
#ifdef copywin
#undef copywin
#endif
int (*foo)(const WINDOW *, WINDOW *, int, int, int, int, int, int, int) = copywin;
int main(void) { return 0; }

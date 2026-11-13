/*[SUSv2]*/
#include <curses.h>
#ifdef wattroff
#undef wattroff
#endif
int (*foo)(WINDOW *, int) = wattroff;
int main(void) { return 0; }

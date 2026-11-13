/*[SUSv2]*/
#include <curses.h>
#ifdef winsdelln
#undef winsdelln
#endif
int (*foo)(WINDOW *, int) = winsdelln;
int main(void) { return 0; }

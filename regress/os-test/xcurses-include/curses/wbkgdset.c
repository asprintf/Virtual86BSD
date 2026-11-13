/*[SUSv2]*/
#include <curses.h>
#ifdef wbkgdset
#undef wbkgdset
#endif
void (*foo)(WINDOW *, chtype) = wbkgdset;
int main(void) { return 0; }

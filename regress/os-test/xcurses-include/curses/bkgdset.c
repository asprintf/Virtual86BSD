/*[SUSv2]*/
#include <curses.h>
#ifdef bkgdset
#undef bkgdset
#endif
void (*foo)(chtype) = bkgdset;
int main(void) { return 0; }

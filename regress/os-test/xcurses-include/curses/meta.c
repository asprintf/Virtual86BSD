/*[SUSv2]*/
#include <curses.h>
#ifdef meta
#undef meta
#endif
int (*foo)(WINDOW *, bool) = meta;
int main(void) { return 0; }

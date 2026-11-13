/*[SUSv2]*/
#include <curses.h>
#ifdef wtouchln
#undef wtouchln
#endif
int (*foo)(WINDOW *, int, int, int) = wtouchln;
int main(void) { return 0; }

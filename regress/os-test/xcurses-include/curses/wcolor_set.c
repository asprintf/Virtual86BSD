/*[SUSv2]*/
#include <curses.h>
#ifdef wcolor_set
#undef wcolor_set
#endif
int (*foo)(WINDOW *, short, void *) = wcolor_set;
int main(void) { return 0; }

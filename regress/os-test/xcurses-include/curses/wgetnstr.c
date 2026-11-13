/*[SUSv2]*/
#include <curses.h>
#ifdef wgetnstr
#undef wgetnstr
#endif
int (*foo)(WINDOW *, char *, int) = wgetnstr;
int main(void) { return 0; }

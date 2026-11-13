/*[SUSv2]*/
#include <curses.h>
#ifdef waddnstr
#undef waddnstr
#endif
int (*foo)(WINDOW *, const char *, int) = waddnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef waddchstr
#undef waddchstr
#endif
int (*foo)(WINDOW *, const chtype *) = waddchstr;
int main(void) { return 0; }

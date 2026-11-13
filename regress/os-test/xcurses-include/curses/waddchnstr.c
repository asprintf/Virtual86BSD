/*[SUSv2]*/
#include <curses.h>
#ifdef waddchnstr
#undef waddchnstr
#endif
int (*foo)(WINDOW *, const chtype *, int) = waddchnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wgetstr
#undef wgetstr
#endif
int (*foo)(WINDOW *, char *) = wgetstr;
int main(void) { return 0; }

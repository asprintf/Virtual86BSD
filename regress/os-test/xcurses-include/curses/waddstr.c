/*[SUSv2]*/
#include <curses.h>
#ifdef waddstr
#undef waddstr
#endif
int (*foo)(WINDOW *, const char *) = waddstr;
int main(void) { return 0; }

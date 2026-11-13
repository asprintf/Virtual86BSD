/*[SUSv2]*/
#include <curses.h>
#ifdef winsstr
#undef winsstr
#endif
int (*foo)(WINDOW *, const char *) = winsstr;
int main(void) { return 0; }

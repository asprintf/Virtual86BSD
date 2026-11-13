/*[SUSv2]*/
#include <curses.h>
#ifdef winnstr
#undef winnstr
#endif
int (*foo)(WINDOW *, char *, int) = winnstr;
int main(void) { return 0; }

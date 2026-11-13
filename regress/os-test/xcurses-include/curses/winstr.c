/*[SUSv2]*/
#include <curses.h>
#ifdef winstr
#undef winstr
#endif
int (*foo)(WINDOW *, char *) = winstr;
int main(void) { return 0; }

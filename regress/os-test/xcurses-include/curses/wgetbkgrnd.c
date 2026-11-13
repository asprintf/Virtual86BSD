/*[SUSv2]*/
#include <curses.h>
#ifdef wgetbkgrnd
#undef wgetbkgrnd
#endif
int (*foo)(WINDOW *, cchar_t *) = wgetbkgrnd;
int main(void) { return 0; }

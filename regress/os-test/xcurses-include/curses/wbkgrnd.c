/*[SUSv2]*/
#include <curses.h>
#ifdef wbkgrnd
#undef wbkgrnd
#endif
int (*foo)(WINDOW *, const cchar_t *) = wbkgrnd;
int main(void) { return 0; }

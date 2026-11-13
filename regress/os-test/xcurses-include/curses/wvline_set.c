/*[SUSv2]*/
#include <curses.h>
#ifdef wvline_set
#undef wvline_set
#endif
int (*foo)(WINDOW *, const cchar_t *, int) = wvline_set;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wchgat
#undef wchgat
#endif
int (*foo)(WINDOW *, int, attr_t, short, const void *) = wchgat;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wattr_off
#undef wattr_off
#endif
int (*foo)(WINDOW *, attr_t, void *) = wattr_off;
int main(void) { return 0; }

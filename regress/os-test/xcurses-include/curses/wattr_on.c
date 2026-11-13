/*[SUSv2]*/
#include <curses.h>
#ifdef wattr_on
#undef wattr_on
#endif
int (*foo)(WINDOW *, attr_t, void *) = wattr_on;
int main(void) { return 0; }

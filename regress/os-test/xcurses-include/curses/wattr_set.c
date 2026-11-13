/*[SUSv2]*/
#include <curses.h>
#ifdef wattr_set
#undef wattr_set
#endif
int (*foo)(WINDOW *, attr_t, short, void *) = wattr_set;
int main(void) { return 0; }

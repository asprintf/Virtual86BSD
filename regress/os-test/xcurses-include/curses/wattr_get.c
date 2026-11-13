/*[SUSv2]*/
#include <curses.h>
#ifdef wattr_get
#undef wattr_get
#endif
int (*foo)(WINDOW *, attr_t *, short *, void *) = wattr_get;
int main(void) { return 0; }

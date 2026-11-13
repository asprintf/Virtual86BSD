/*[SUSv2]*/
#include <curses.h>
#ifdef winsch
#undef winsch
#endif
int (*foo)(WINDOW *, chtype) = winsch;
int main(void) { return 0; }

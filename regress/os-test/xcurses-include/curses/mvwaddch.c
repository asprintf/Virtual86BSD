/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddch
#undef mvwaddch
#endif
int (*foo)(WINDOW *, int, int, const chtype) = mvwaddch;
int main(void) { return 0; }

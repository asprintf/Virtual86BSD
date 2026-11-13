/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddch
#undef mvaddch
#endif
int (*foo)(int, int, const chtype) = mvaddch;
int main(void) { return 0; }

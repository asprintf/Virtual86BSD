/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinch
#undef mvwinch
#endif
chtype (*foo)(WINDOW *, int, int) = mvwinch;
int main(void) { return 0; }

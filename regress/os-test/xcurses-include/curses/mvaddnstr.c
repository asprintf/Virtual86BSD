/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddnstr
#undef mvaddnstr
#endif
int (*foo)(int, int, const char *, int) = mvaddnstr;
int main(void) { return 0; }

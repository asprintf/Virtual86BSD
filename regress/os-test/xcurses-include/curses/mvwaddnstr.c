/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddnstr
#undef mvwaddnstr
#endif
int (*foo)(WINDOW *, int, int, const char *, int) = mvwaddnstr;
int main(void) { return 0; }

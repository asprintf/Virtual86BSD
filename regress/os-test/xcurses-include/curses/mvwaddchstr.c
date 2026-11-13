/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddchstr
#undef mvwaddchstr
#endif
int (*foo)(WINDOW *, int, int, const chtype *) = mvwaddchstr;
int main(void) { return 0; }

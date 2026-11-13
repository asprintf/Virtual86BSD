/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddchstr
#undef mvaddchstr
#endif
int (*foo)(int, int, const chtype *) = mvaddchstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddchnstr
#undef mvaddchnstr
#endif
int (*foo)(int, int, const chtype *, int) = mvaddchnstr;
int main(void) { return 0; }

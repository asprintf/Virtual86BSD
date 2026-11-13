/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddchnstr
#undef mvwaddchnstr
#endif
int (*foo)(WINDOW *, int, int, const chtype *, int) = mvwaddchnstr;
int main(void) { return 0; }

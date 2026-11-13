/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinchnstr
#undef mvwinchnstr
#endif
int (*foo)(WINDOW *, int, int, chtype *, int) = mvwinchnstr;
int main(void) { return 0; }

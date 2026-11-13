/*[SUSv2]*/
#include <curses.h>
#ifdef mvinchnstr
#undef mvinchnstr
#endif
int (*foo)(int, int, chtype *, int) = mvinchnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvwhline
#undef mvwhline
#endif
int (*foo)(WINDOW *, int, int, chtype, int) = mvwhline;
int main(void) { return 0; }

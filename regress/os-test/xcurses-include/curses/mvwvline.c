/*[SUSv2]*/
#include <curses.h>
#ifdef mvwvline
#undef mvwvline
#endif
int (*foo)(WINDOW *, int, int, chtype, int) = mvwvline;
int main(void) { return 0; }

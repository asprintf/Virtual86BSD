/*[SUSv2]*/
#include <curses.h>
#ifdef mvwdelch
#undef mvwdelch
#endif
int (*foo)(WINDOW *, int, int) = mvwdelch;
int main(void) { return 0; }

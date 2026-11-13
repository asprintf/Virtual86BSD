/*[SUSv2]*/
#include <curses.h>
#ifdef mvwgetch
#undef mvwgetch
#endif
int (*foo)(WINDOW *, int, int) = mvwgetch;
int main(void) { return 0; }

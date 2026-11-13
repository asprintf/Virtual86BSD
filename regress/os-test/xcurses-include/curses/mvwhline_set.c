/*[SUSv2]*/
#include <curses.h>
#ifdef mvwhline_set
#undef mvwhline_set
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *, int) = mvwhline_set;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvwvline_set
#undef mvwvline_set
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *, int) = mvwvline_set;
int main(void) { return 0; }

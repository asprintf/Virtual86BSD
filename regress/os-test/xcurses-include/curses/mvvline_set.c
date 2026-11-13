/*[SUSv2]*/
#include <curses.h>
#ifdef mvvline_set
#undef mvvline_set
#endif
int (*foo)(int, int, const cchar_t *, int) = mvvline_set;
int main(void) { return 0; }

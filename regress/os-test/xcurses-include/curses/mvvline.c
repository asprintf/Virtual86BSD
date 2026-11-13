/*[SUSv2]*/
#include <curses.h>
#ifdef mvvline
#undef mvvline
#endif
int (*foo)(int, int, chtype, int) = mvvline;
int main(void) { return 0; }

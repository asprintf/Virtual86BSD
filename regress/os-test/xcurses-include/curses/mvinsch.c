/*[SUSv2]*/
#include <curses.h>
#ifdef mvinsch
#undef mvinsch
#endif
int (*foo)(int, int, chtype) = mvinsch;
int main(void) { return 0; }

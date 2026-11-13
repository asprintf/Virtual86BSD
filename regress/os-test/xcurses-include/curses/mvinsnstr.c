/*[SUSv2]*/
#include <curses.h>
#ifdef mvinsnstr
#undef mvinsnstr
#endif
int (*foo)(int, int, const char *, int) = mvinsnstr;
int main(void) { return 0; }

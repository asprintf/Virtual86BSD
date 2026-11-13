/*[SUSv2]*/
#include <curses.h>
#ifdef mvinchstr
#undef mvinchstr
#endif
int (*foo)(int, int, chtype *) = mvinchstr;
int main(void) { return 0; }

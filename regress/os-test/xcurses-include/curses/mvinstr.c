/*[SUSv2]*/
#include <curses.h>
#ifdef mvinstr
#undef mvinstr
#endif
int (*foo)(int, int, char *) = mvinstr;
int main(void) { return 0; }

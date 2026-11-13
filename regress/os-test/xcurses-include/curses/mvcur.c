/*[SUSv2]*/
#include <curses.h>
#ifdef mvcur
#undef mvcur
#endif
int (*foo)(int, int, int, int) = mvcur;
int main(void) { return 0; }

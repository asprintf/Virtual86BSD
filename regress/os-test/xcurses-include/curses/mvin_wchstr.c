/*[SUSv2]*/
#include <curses.h>
#ifdef mvin_wchstr
#undef mvin_wchstr
#endif
int (*foo)(int, int, cchar_t *) = mvin_wchstr;
int main(void) { return 0; }

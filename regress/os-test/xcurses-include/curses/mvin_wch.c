/*[SUSv2]*/
#include <curses.h>
#ifdef mvin_wch
#undef mvin_wch
#endif
int (*foo)(int, int, cchar_t *) = mvin_wch;
int main(void) { return 0; }

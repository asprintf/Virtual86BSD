/*[SUSv2]*/
#include <curses.h>
#ifdef mvin_wchnstr
#undef mvin_wchnstr
#endif
int (*foo)(int, int, cchar_t *, int) = mvin_wchnstr;
int main(void) { return 0; }

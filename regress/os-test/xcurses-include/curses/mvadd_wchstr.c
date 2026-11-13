/*[SUSv2]*/
#include <curses.h>
#ifdef mvadd_wchstr
#undef mvadd_wchstr
#endif
int (*foo)(int, int, const cchar_t *) = mvadd_wchstr;
int main(void) { return 0; }

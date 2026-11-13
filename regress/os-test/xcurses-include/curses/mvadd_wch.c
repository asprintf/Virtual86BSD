/*[SUSv2]*/
#include <curses.h>
#ifdef mvadd_wch
#undef mvadd_wch
#endif
int (*foo)(int, int, const cchar_t *) = mvadd_wch;
int main(void) { return 0; }

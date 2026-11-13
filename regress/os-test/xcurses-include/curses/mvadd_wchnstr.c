/*[SUSv2]*/
#include <curses.h>
#ifdef mvadd_wchnstr
#undef mvadd_wchnstr
#endif
int (*foo)(int, int, const cchar_t *, int) = mvadd_wchnstr;
int main(void) { return 0; }

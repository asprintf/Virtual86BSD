/*[SUSv2]*/
#include <curses.h>
#ifdef wadd_wchnstr
#undef wadd_wchnstr
#endif
int (*foo)(WINDOW *, const cchar_t *, int) = wadd_wchnstr;
int main(void) { return 0; }

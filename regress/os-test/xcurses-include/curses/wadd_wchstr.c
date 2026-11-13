/*[SUSv2]*/
#include <curses.h>
#ifdef wadd_wchstr
#undef wadd_wchstr
#endif
int (*foo)(WINDOW *, const cchar_t *) = wadd_wchstr;
int main(void) { return 0; }

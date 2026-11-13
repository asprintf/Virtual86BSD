/*[SUSv2]*/
#include <curses.h>
#ifdef wadd_wch
#undef wadd_wch
#endif
int (*foo)(WINDOW *, const cchar_t *) = wadd_wch;
int main(void) { return 0; }

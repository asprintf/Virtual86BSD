/*[SUSv2]*/
#include <curses.h>
#ifdef win_wchstr
#undef win_wchstr
#endif
int (*foo)(WINDOW *, cchar_t *) = win_wchstr;
int main(void) { return 0; }

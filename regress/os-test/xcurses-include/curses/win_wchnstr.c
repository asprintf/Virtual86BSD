/*[SUSv2]*/
#include <curses.h>
#ifdef win_wchnstr
#undef win_wchnstr
#endif
int (*foo)(WINDOW *, cchar_t *, int) = win_wchnstr;
int main(void) { return 0; }

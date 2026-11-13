/*[SUSv2]*/
#include <curses.h>
#ifdef win_wch
#undef win_wch
#endif
int (*foo)(WINDOW *, cchar_t *) = win_wch;
int main(void) { return 0; }

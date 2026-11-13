/*[SUSv2]*/
#include <curses.h>
#ifdef wecho_wchar
#undef wecho_wchar
#endif
int (*foo)(WINDOW *, const cchar_t *) = wecho_wchar;
int main(void) { return 0; }

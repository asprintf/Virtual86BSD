/*[SUSv2]*/
#include <curses.h>
#ifdef waddnwstr
#undef waddnwstr
#endif
int (*foo)(WINDOW *, const wchar_t *, int) = waddnwstr;
int main(void) { return 0; }

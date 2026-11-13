/*[SUSv2]*/
#include <curses.h>
#ifdef waddwstr
#undef waddwstr
#endif
int (*foo)(WINDOW *, const wchar_t *) = waddwstr;
int main(void) { return 0; }

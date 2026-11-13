/*[SUSv2]*/
#include <curses.h>
#ifdef mvaddnwstr
#undef mvaddnwstr
#endif
int (*foo)(int, int, const wchar_t *, int) = mvaddnwstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvwaddnwstr
#undef mvwaddnwstr
#endif
int (*foo)(WINDOW *, int, int, const wchar_t *, int) = mvwaddnwstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinnwstr
#undef mvwinnwstr
#endif
int (*foo)(WINDOW *, int, int, wchar_t *, int) = mvwinnwstr;
int main(void) { return 0; }

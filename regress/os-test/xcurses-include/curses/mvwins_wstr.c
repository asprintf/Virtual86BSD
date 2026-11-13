/*[SUSv2]*/
#include <curses.h>
#ifdef mvwins_wstr
#undef mvwins_wstr
#endif
int (*foo)(WINDOW *, int, int, const wchar_t *) = mvwins_wstr;
int main(void) { return 0; }

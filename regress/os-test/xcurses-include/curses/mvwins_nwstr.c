/*[SUSv2]*/
#include <curses.h>
#ifdef mvwins_nwstr
#undef mvwins_nwstr
#endif
int (*foo)(WINDOW *, int, int, const wchar_t *, int) = mvwins_nwstr;
int main(void) { return 0; }

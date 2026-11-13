/*[SUSv2]*/
#include <curses.h>
#ifdef winwstr
#undef winwstr
#endif
int (*foo)(WINDOW *, wchar_t *) = winwstr;
int main(void) { return 0; }

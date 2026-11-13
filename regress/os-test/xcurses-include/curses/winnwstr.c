/*[SUSv2]*/
#include <curses.h>
#ifdef winnwstr
#undef winnwstr
#endif
int (*foo)(WINDOW *, wchar_t *, int) = winnwstr;
int main(void) { return 0; }

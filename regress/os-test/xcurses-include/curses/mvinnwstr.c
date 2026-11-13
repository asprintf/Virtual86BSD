/*[SUSv2]*/
#include <curses.h>
#ifdef mvinnwstr
#undef mvinnwstr
#endif
int (*foo)(int, int, wchar_t *, int) = mvinnwstr;
int main(void) { return 0; }

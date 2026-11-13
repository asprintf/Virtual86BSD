/*[SUSv2]*/
#include <curses.h>
#ifdef mvins_nwstr
#undef mvins_nwstr
#endif
int (*foo)(int, int, const wchar_t *, int) = mvins_nwstr;
int main(void) { return 0; }

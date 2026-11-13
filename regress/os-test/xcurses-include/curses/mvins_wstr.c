/*[SUSv2]*/
#include <curses.h>
#ifdef mvins_wstr
#undef mvins_wstr
#endif
int (*foo)(int, int, const wchar_t *) = mvins_wstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef slk_wset
#undef slk_wset
#endif
int (*foo)(int, const wchar_t *, int) = slk_wset;
int main(void) { return 0; }

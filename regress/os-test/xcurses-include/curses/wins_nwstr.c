/*[SUSv2]*/
#include <curses.h>
#ifdef wins_nwstr
#undef wins_nwstr
#endif
int (*foo)(WINDOW *, const wchar_t *, int) = wins_nwstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wins_wstr
#undef wins_wstr
#endif
int (*foo)(WINDOW *, const wchar_t *) = wins_wstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef slk_noutrefresh
#undef slk_noutrefresh
#endif
int (*foo)(void) = slk_noutrefresh;
int main(void) { return 0; }

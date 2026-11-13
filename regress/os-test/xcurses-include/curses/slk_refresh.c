/*[SUSv2]*/
#include <curses.h>
#ifdef slk_refresh
#undef slk_refresh
#endif
int (*foo)(void) = slk_refresh;
int main(void) { return 0; }

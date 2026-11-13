/*[SUSv2]*/
#include <curses.h>
#ifdef slk_restore
#undef slk_restore
#endif
int (*foo)(void) = slk_restore;
int main(void) { return 0; }

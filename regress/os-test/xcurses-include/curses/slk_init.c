/*[SUSv2]*/
#include <curses.h>
#ifdef slk_init
#undef slk_init
#endif
int (*foo)(int) = slk_init;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef slk_clear
#undef slk_clear
#endif
int (*foo)(void) = slk_clear;
int main(void) { return 0; }

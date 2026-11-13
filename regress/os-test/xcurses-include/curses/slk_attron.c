/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attron
#undef slk_attron
#endif
int (*foo)(const chtype) = slk_attron;
int main(void) { return 0; }

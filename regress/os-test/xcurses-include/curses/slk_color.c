/*[SUSv2]*/
#include <curses.h>
#ifdef slk_color
#undef slk_color
#endif
int (*foo)(short) = slk_color;
int main(void) { return 0; }

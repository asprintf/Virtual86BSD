/*[SUSv2]*/
#include <curses.h>
#ifdef slk_touch
#undef slk_touch
#endif
int (*foo)(void) = slk_touch;
int main(void) { return 0; }

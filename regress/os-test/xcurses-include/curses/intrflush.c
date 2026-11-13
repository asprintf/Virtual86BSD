/*[SUSv2]*/
#include <curses.h>
#ifdef intrflush
#undef intrflush
#endif
int (*foo)(WINDOW *, bool) = intrflush;
int main(void) { return 0; }

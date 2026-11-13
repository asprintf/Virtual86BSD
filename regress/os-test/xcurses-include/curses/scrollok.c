/*[SUSv2]*/
#include <curses.h>
#ifdef scrollok
#undef scrollok
#endif
int (*foo)(WINDOW *, bool) = scrollok;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wsyncdown
#undef wsyncdown
#endif
void (*foo)(WINDOW *) = wsyncdown;
int main(void) { return 0; }

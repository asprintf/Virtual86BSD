/*[SUSv2]*/
#include <curses.h>
#ifdef overlay
#undef overlay
#endif
int (*foo)(const WINDOW *, WINDOW *) = overlay;
int main(void) { return 0; }

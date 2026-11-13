/*[SUSv2]*/
#include <curses.h>
#ifdef start_color
#undef start_color
#endif
int (*foo)(void) = start_color;
int main(void) { return 0; }

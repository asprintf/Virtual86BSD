/*[SUSv2]*/
#include <curses.h>
#ifdef filter
#undef filter
#endif
void (*foo)(void) = filter;
int main(void) { return 0; }

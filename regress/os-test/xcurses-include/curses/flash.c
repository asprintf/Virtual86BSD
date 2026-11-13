/*[SUSv2]*/
#include <curses.h>
#ifdef flash
#undef flash
#endif
int (*foo)(void) = flash;
int main(void) { return 0; }

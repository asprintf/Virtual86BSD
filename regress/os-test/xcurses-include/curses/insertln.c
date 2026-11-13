/*[SUSv2]*/
#include <curses.h>
#ifdef insertln
#undef insertln
#endif
int (*foo)(void) = insertln;
int main(void) { return 0; }

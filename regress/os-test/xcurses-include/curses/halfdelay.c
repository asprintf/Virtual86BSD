/*[SUSv2]*/
#include <curses.h>
#ifdef halfdelay
#undef halfdelay
#endif
int (*foo)(int) = halfdelay;
int main(void) { return 0; }

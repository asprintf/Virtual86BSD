/*[SUSv2]*/
#include <curses.h>
#ifdef standout
#undef standout
#endif
int (*foo)(void) = standout;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef standend
#undef standend
#endif
int (*foo)(void) = standend;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef erase
#undef erase
#endif
int (*foo)(void) = erase;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef delch
#undef delch
#endif
int (*foo)(void) = delch;
int main(void) { return 0; }

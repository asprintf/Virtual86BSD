/*[SUSv2]*/
#include <curses.h>
#ifdef napms
#undef napms
#endif
int (*foo)(int) = napms;
int main(void) { return 0; }

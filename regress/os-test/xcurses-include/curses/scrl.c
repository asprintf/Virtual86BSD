/*[SUSv2]*/
#include <curses.h>
#ifdef scrl
#undef scrl
#endif
int (*foo)(int) = scrl;
int main(void) { return 0; }

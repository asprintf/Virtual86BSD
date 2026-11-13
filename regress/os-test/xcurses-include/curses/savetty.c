/*[SUSv2]*/
#include <curses.h>
#ifdef savetty
#undef savetty
#endif
int (*foo)(void) = savetty;
int main(void) { return 0; }

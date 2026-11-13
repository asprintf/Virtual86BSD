/*[SUSv2]*/
#include <curses.h>
#ifdef flushinp
#undef flushinp
#endif
int (*foo)(void) = flushinp;
int main(void) { return 0; }

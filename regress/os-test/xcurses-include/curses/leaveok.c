/*[SUSv2]*/
#include <curses.h>
#ifdef leaveok
#undef leaveok
#endif
int (*foo)(WINDOW *, bool) = leaveok;
int main(void) { return 0; }

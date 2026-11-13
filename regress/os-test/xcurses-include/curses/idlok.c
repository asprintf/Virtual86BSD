/*[SUSv2]*/
#include <curses.h>
#ifdef idlok
#undef idlok
#endif
int (*foo)(WINDOW *, bool) = idlok;
int main(void) { return 0; }

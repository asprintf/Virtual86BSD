/*[SUSv2]*/
#include <curses.h>
#ifdef idcok
#undef idcok
#endif
void (*foo)(WINDOW *, bool) = idcok;
int main(void) { return 0; }

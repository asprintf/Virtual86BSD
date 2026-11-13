/*[SUSv2]*/
#include <curses.h>
#ifdef wsyncup
#undef wsyncup
#endif
void (*foo)(WINDOW *) = wsyncup;
int main(void) { return 0; }

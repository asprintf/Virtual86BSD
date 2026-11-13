/*[SUSv2]*/
#include <curses.h>
#ifdef wtimeout
#undef wtimeout
#endif
void (*foo)(WINDOW *, int) = wtimeout;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wstandout
#undef wstandout
#endif
int (*foo)(WINDOW *) = wstandout;
int main(void) { return 0; }

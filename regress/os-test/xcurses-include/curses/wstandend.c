/*[SUSv2]*/
#include <curses.h>
#ifdef wstandend
#undef wstandend
#endif
int (*foo)(WINDOW *) = wstandend;
int main(void) { return 0; }

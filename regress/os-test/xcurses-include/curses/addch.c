/*[SUSv2]*/
#include <curses.h>
#ifdef addch
#undef addch
#endif
int (*foo)(const chtype) = addch;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef pechochar
#undef pechochar
#endif
int (*foo)(WINDOW *, chtype) = pechochar;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef border
#undef border
#endif
int (*foo)(chtype, chtype, chtype, chtype, chtype, chtype, chtype, chtype) = border;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef box
#undef box
#endif
int (*foo)(WINDOW *, chtype, chtype) = box;
int main(void) { return 0; }

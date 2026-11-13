/*[SUSv2]*/
#include <curses.h>
#ifdef inchstr
#undef inchstr
#endif
int (*foo)(chtype *) = inchstr;
int main(void) { return 0; }

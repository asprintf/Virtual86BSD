/*[SUSv2]*/
#include <curses.h>
#ifdef winchstr
#undef winchstr
#endif
int (*foo)(WINDOW *, chtype *) = winchstr;
int main(void) { return 0; }

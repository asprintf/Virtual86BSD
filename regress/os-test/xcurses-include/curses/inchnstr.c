/*[SUSv2]*/
#include <curses.h>
#ifdef inchnstr
#undef inchnstr
#endif
int (*foo)(chtype *, int) = inchnstr;
int main(void) { return 0; }

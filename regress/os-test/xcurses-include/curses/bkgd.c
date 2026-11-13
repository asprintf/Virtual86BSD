/*[SUSv2]*/
#include <curses.h>
#ifdef bkgd
#undef bkgd
#endif
int (*foo)(chtype) = bkgd;
int main(void) { return 0; }

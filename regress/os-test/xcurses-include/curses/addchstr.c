/*[SUSv2]*/
#include <curses.h>
#ifdef addchstr
#undef addchstr
#endif
int (*foo)(const chtype *) = addchstr;
int main(void) { return 0; }

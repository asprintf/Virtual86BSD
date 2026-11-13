/*[SUSv2]*/
#include <curses.h>
#ifdef curs_set
#undef curs_set
#endif
int (*foo)(int) = curs_set;
int main(void) { return 0; }

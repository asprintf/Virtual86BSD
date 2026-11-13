/*[SUSv2]*/
#include <curses.h>
#ifdef attroff
#undef attroff
#endif
int (*foo)(int) = attroff;
int main(void) { return 0; }

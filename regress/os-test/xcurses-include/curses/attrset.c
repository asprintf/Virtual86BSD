/*[SUSv2]*/
#include <curses.h>
#ifdef attrset
#undef attrset
#endif
int (*foo)(int) = attrset;
int main(void) { return 0; }

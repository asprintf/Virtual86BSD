/*[SUSv2]*/
#include <curses.h>
#ifdef wattrset
#undef wattrset
#endif
int (*foo)(WINDOW *, int) = wattrset;
int main(void) { return 0; }

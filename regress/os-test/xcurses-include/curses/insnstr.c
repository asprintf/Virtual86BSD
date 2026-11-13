/*[SUSv2]*/
#include <curses.h>
#ifdef insnstr
#undef insnstr
#endif
int (*foo)(const char *, int) = insnstr;
int main(void) { return 0; }

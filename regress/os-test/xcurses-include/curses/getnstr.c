/*[SUSv2]*/
#include <curses.h>
#ifdef getnstr
#undef getnstr
#endif
int (*foo)(char *, int) = getnstr;
int main(void) { return 0; }

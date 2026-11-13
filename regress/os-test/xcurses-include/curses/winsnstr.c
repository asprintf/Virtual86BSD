/*[SUSv2]*/
#include <curses.h>
#ifdef winsnstr
#undef winsnstr
#endif
int (*foo)(WINDOW *, const char *, int) = winsnstr;
int main(void) { return 0; }

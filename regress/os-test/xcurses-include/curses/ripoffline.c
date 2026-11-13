/*[SUSv2]*/
#include <curses.h>
#ifdef ripoffline
#undef ripoffline
#endif
int (*foo)(int, int (*)(WINDOW *, int)) = ripoffline;
int main(void) { return 0; }

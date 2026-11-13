/*[SUSv2]*/
#include <curses.h>
#ifdef wnoutrefresh
#undef wnoutrefresh
#endif
int (*foo)(WINDOW *) = wnoutrefresh;
int main(void) { return 0; }

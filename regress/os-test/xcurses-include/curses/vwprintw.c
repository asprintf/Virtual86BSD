/*[SUSv2]*/
#include <curses.h>
#ifdef vwprintw
#undef vwprintw
#endif
int (*foo)(WINDOW *, char *, va_list) = vwprintw;
int main(void) { return 0; }

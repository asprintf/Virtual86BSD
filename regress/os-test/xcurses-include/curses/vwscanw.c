/*[SUSv2]*/
#include <curses.h>
#ifdef vwscanw
#undef vwscanw
#endif
int (*foo)(WINDOW *, char *, va_list) = vwscanw;
int main(void) { return 0; }

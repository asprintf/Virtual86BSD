/*[SUSv2]*/
#include <curses.h>
#ifdef mvwin_wchstr
#undef mvwin_wchstr
#endif
int (*foo)(WINDOW *, int, int, cchar_t *) = mvwin_wchstr;
int main(void) { return 0; }

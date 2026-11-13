/*[SUSv2]*/
#include <curses.h>
#ifdef mvwin_wch
#undef mvwin_wch
#endif
int (*foo)(WINDOW *, int, int, cchar_t *) = mvwin_wch;
int main(void) { return 0; }

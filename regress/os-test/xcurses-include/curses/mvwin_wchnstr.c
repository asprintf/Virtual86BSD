/*[SUSv2]*/
#include <curses.h>
#ifdef mvwin_wchnstr
#undef mvwin_wchnstr
#endif
int (*foo)(WINDOW *, int, int, cchar_t *, int) = mvwin_wchnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvwins_wch
#undef mvwins_wch
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *) = mvwins_wch;
int main(void) { return 0; }

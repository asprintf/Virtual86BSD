/*[SUSv2]*/
#include <curses.h>
#ifdef mvwadd_wch
#undef mvwadd_wch
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *) = mvwadd_wch;
int main(void) { return 0; }

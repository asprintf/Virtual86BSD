/*[SUSv2]*/
#include <curses.h>
#ifdef mvwadd_wchstr
#undef mvwadd_wchstr
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *) = mvwadd_wchstr;
int main(void) { return 0; }

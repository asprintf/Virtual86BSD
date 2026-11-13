/*[SUSv2]*/
#include <curses.h>
#ifdef mvwadd_wchnstr
#undef mvwadd_wchnstr
#endif
int (*foo)(WINDOW *, int, int, const cchar_t *, int) = mvwadd_wchnstr;
int main(void) { return 0; }

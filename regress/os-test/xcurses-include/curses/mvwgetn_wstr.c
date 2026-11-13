/*[SUSv2]*/
#include <curses.h>
#ifdef mvwgetn_wstr
#undef mvwgetn_wstr
#endif
int (*foo)(WINDOW *, int, int, wint_t *, int) = mvwgetn_wstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef wgetn_wstr
#undef wgetn_wstr
#endif
int (*foo)(WINDOW *, wint_t *, int) = wgetn_wstr;
int main(void) { return 0; }

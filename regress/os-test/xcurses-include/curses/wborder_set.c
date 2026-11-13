/*[SUSv2]*/
#include <curses.h>
#ifdef wborder_set
#undef wborder_set
#endif
int (*foo)(WINDOW *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *, const cchar_t *) = wborder_set;
int main(void) { return 0; }

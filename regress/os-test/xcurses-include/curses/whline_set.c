/*[SUSv2]*/
#include <curses.h>
#ifdef whline_set
#undef whline_set
#endif
int (*foo)(WINDOW *, const cchar_t *, int) = whline_set;
int main(void) { return 0; }

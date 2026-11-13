/*[SUSv2]*/
#include <curses.h>
#ifdef add_wchnstr
#undef add_wchnstr
#endif
int (*foo)(const cchar_t *, int) = add_wchnstr;
int main(void) { return 0; }

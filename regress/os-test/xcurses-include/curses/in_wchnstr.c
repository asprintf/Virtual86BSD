/*[SUSv2]*/
#include <curses.h>
#ifdef in_wchnstr
#undef in_wchnstr
#endif
int (*foo)(cchar_t *, int) = in_wchnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef in_wchstr
#undef in_wchstr
#endif
int (*foo)(cchar_t *) = in_wchstr;
int main(void) { return 0; }

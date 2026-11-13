/*[SUSv2]*/
#include <curses.h>
#ifdef add_wchstr
#undef add_wchstr
#endif
int (*foo)(const cchar_t *) = add_wchstr;
int main(void) { return 0; }

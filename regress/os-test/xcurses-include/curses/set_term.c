/*[SUSv2]*/
#include <curses.h>
#ifdef set_term
#undef set_term
#endif
SCREEN *(*foo)(SCREEN *) = set_term;
int main(void) { return 0; }

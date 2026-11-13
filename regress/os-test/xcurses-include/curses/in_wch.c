/*[SUSv2]*/
#include <curses.h>
#ifdef in_wch
#undef in_wch
#endif
int (*foo)(cchar_t *) = in_wch;
int main(void) { return 0; }

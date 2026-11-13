/*[SUSv2]*/
#include <curses.h>
#ifdef add_wch
#undef add_wch
#endif
int (*foo)(const cchar_t *) = add_wch;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef def_prog_mode
#undef def_prog_mode
#endif
int (*foo)(void) = def_prog_mode;
int main(void) { return 0; }

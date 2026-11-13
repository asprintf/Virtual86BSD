/*[SUSv2]*/
#include <curses.h>
#ifdef reset_prog_mode
#undef reset_prog_mode
#endif
int (*foo)(void) = reset_prog_mode;
int main(void) { return 0; }

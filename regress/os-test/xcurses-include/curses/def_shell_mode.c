/*[SUSv2]*/
#include <curses.h>
#ifdef def_shell_mode
#undef def_shell_mode
#endif
int (*foo)(void) = def_shell_mode;
int main(void) { return 0; }

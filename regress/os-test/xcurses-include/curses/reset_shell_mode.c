/*[SUSv2]*/
#include <curses.h>
#ifdef reset_shell_mode
#undef reset_shell_mode
#endif
int (*foo)(void) = reset_shell_mode;
int main(void) { return 0; }

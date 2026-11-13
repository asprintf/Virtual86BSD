/*[SUSv2]*/
#include <curses.h>
#ifdef touchwin
#undef touchwin
#endif
int (*foo)(WINDOW *) = touchwin;
int main(void) { return 0; }

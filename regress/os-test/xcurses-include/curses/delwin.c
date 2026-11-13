/*[SUSv2]*/
#include <curses.h>
#ifdef delwin
#undef delwin
#endif
int (*foo)(WINDOW *) = delwin;
int main(void) { return 0; }

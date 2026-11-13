/*[SUSv2]*/
#include <curses.h>
#ifdef scr_init
#undef scr_init
#endif
int (*foo)(const char *) = scr_init;
int main(void) { return 0; }

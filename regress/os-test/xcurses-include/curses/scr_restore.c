/*[SUSv2]*/
#include <curses.h>
#ifdef scr_restore
#undef scr_restore
#endif
int (*foo)(const char *) = scr_restore;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef scr_set
#undef scr_set
#endif
int (*foo)(const char *) = scr_set;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef scr_dump
#undef scr_dump
#endif
int (*foo)(const char *) = scr_dump;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef addstr
#undef addstr
#endif
int (*foo)(const char *) = addstr;
int main(void) { return 0; }

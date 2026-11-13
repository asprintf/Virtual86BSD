/*[SUSv2]*/
#include <curses.h>
#ifdef addnstr
#undef addnstr
#endif
int (*foo)(const char *, int) = addnstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef getstr
#undef getstr
#endif
int (*foo)(char *) = getstr;
int main(void) { return 0; }

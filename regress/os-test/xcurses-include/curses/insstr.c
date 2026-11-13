/*[SUSv2]*/
#include <curses.h>
#ifdef insstr
#undef insstr
#endif
int (*foo)(const char *) = insstr;
int main(void) { return 0; }

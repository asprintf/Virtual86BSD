/*[SUSv2]*/
#include <curses.h>
#ifdef termname
#undef termname
#endif
char *(*foo)(void) = termname;
int main(void) { return 0; }

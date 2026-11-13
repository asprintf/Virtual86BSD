/*[SUSv2]*/
#include <curses.h>
#ifdef innstr
#undef innstr
#endif
int (*foo)(char *, int) = innstr;
int main(void) { return 0; }

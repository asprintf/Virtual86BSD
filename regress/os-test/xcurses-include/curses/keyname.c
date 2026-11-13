/*[SUSv2]*/
#include <curses.h>
#ifdef keyname
#undef keyname
#endif
char *(*foo)(int) = keyname;
int main(void) { return 0; }

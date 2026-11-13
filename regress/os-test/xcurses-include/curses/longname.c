/*[SUSv2]*/
#include <curses.h>
#ifdef longname
#undef longname
#endif
char *(*foo)(void) = longname;
int main(void) { return 0; }

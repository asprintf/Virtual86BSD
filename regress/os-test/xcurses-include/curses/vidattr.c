/*[SUSv2]*/
#include <curses.h>
#ifdef vidattr
#undef vidattr
#endif
int (*foo)(chtype) = vidattr;
int main(void) { return 0; }

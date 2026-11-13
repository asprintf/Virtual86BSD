/*[SUSv2]*/
#include <curses.h>
#ifdef mvwin
#undef mvwin
#endif
int (*foo)(WINDOW *, int, int) = mvwin;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef untouchwin
#undef untouchwin
#endif
int (*foo)(WINDOW *) = untouchwin;
int main(void) { return 0; }

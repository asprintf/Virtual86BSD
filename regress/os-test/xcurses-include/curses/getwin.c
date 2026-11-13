/*[SUSv2]*/
#include <curses.h>
#ifdef getwin
#undef getwin
#endif
WINDOW *(*foo)(FILE *) = getwin;
int main(void) { return 0; }

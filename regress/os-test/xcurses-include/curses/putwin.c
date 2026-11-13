/*[SUSv2]*/
#include <curses.h>
#ifdef putwin
#undef putwin
#endif
int (*foo)(WINDOW *, FILE *) = putwin;
int main(void) { return 0; }

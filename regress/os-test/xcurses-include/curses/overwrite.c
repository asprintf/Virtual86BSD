/*[SUSv2]*/
#include <curses.h>
#ifdef overwrite
#undef overwrite
#endif
int (*foo)(const WINDOW *, WINDOW *) = overwrite;
int main(void) { return 0; }

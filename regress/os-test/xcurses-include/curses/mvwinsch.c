/*[SUSv2]*/
#include <curses.h>
#ifdef mvwinsch
#undef mvwinsch
#endif
int (*foo)(WINDOW *, int, int, chtype) = mvwinsch;
int main(void) { return 0; }

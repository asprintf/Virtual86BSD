/*[SUSv2]*/
#include <curses.h>
#ifdef mvderwin
#undef mvderwin
#endif
int (*foo)(WINDOW *, int, int) = mvderwin;
int main(void) { return 0; }

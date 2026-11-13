/*[SUSv2]*/
#include <curses.h>
#ifdef mvhline
#undef mvhline
#endif
int (*foo)(int, int, chtype, int) = mvhline;
int main(void) { return 0; }

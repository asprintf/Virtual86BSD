/*[SUSv2]*/
#include <curses.h>
#ifdef mvdelch
#undef mvdelch
#endif
int (*foo)(int, int) = mvdelch;
int main(void) { return 0; }

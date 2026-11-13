/*[SUSv2]*/
#include <curses.h>
#ifdef setscrreg
#undef setscrreg
#endif
int (*foo)(int, int) = setscrreg;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef mvgetch
#undef mvgetch
#endif
int (*foo)(int, int) = mvgetch;
int main(void) { return 0; }

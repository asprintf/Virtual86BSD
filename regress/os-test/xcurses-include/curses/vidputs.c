/*[SUSv2]*/
#include <curses.h>
#ifdef vidputs
#undef vidputs
#endif
int (*foo)(chtype, int (*)(int)) = vidputs;
int main(void) { return 0; }

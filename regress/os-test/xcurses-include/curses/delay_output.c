/*[SUSv2]*/
#include <curses.h>
#ifdef delay_output
#undef delay_output
#endif
int (*foo)(int) = delay_output;
int main(void) { return 0; }

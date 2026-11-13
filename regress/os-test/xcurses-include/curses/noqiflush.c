/*[SUSv2]*/
#include <curses.h>
#ifdef noqiflush
#undef noqiflush
#endif
void (*foo)(void) = noqiflush;
int main(void) { return 0; }

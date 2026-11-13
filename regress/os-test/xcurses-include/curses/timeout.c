/*[SUSv2]*/
#include <curses.h>
#ifdef timeout
#undef timeout
#endif
void (*foo)(int) = timeout;
int main(void) { return 0; }

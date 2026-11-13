/*[SUSv2]*/
#include <curses.h>
#ifdef raw
#undef raw
#endif
int (*foo)(void) = raw;
int main(void) { return 0; }

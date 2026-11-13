/*[SUSv2]*/
#include <curses.h>
#ifdef baudrate
#undef baudrate
#endif
int (*foo)(void) = baudrate;
int main(void) { return 0; }

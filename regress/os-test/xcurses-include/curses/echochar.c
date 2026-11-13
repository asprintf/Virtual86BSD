/*[SUSv2]*/
#include <curses.h>
#ifdef echochar
#undef echochar
#endif
int (*foo)(const chtype) = echochar;
int main(void) { return 0; }

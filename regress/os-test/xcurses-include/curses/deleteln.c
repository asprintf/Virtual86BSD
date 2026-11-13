/*[SUSv2]*/
#include <curses.h>
#ifdef deleteln
#undef deleteln
#endif
int (*foo)(void) = deleteln;
int main(void) { return 0; }

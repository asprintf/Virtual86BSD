/*[SUSv2]*/
#include <curses.h>
#ifdef touchline
#undef touchline
#endif
int (*foo)(WINDOW *, int, int) = touchline;
int main(void) { return 0; }

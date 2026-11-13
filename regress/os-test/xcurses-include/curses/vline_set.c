/*[SUSv2]*/
#include <curses.h>
#ifdef vline_set
#undef vline_set
#endif
int (*foo)(const cchar_t *, int) = vline_set;
int main(void) { return 0; }

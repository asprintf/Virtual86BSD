/*[SUSv2]*/
#include <curses.h>
#ifdef hline_set
#undef hline_set
#endif
int (*foo)(const cchar_t *, int) = hline_set;
int main(void) { return 0; }

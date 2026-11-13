/*[SUSv2]*/
#include <curses.h>
#ifdef attr_on
#undef attr_on
#endif
int (*foo)(attr_t, void *) = attr_on;
int main(void) { return 0; }

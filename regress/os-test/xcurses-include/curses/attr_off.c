/*[SUSv2]*/
#include <curses.h>
#ifdef attr_off
#undef attr_off
#endif
int (*foo)(attr_t, void *) = attr_off;
int main(void) { return 0; }

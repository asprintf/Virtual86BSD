/*[SUSv2]*/
#include <curses.h>
#ifdef attr_set
#undef attr_set
#endif
int (*foo)(attr_t, short, void *) = attr_set;
int main(void) { return 0; }

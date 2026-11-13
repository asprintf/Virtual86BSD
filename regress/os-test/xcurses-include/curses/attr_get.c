/*[SUSv2]*/
#include <curses.h>
#ifdef attr_get
#undef attr_get
#endif
int (*foo)(attr_t *, short *, void *) = attr_get;
int main(void) { return 0; }

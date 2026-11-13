/*[SUSv2]*/
#include <curses.h>
#ifdef chgat
#undef chgat
#endif
int (*foo)(int, attr_t, short, const void *) = chgat;
int main(void) { return 0; }

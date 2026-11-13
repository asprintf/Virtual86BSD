/*[SUSv2]*/
#include <curses.h>
#ifdef mvchgat
#undef mvchgat
#endif
int (*foo)(int, int, int, attr_t, short, const void *) = mvchgat;
int main(void) { return 0; }

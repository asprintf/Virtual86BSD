/*[SUSv2]*/
#include <curses.h>
#ifdef vid_attr
#undef vid_attr
#endif
int (*foo)(attr_t, short, void *) = vid_attr;
int main(void) { return 0; }

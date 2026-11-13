/*[SUSv2]*/
#include <curses.h>
#ifdef vid_puts
#undef vid_puts
#endif
int (*foo)(attr_t, short, void *, int (*)(int)) = vid_puts;
int main(void) { return 0; }

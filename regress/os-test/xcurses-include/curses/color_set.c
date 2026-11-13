/*[SUSv2]*/
#include <curses.h>
#ifdef color_set
#undef color_set
#endif
int (*foo)(short, void *) = color_set;
int main(void) { return 0; }

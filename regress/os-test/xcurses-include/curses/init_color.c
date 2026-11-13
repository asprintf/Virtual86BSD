/*[SUSv2]*/
#include <curses.h>
#ifdef init_color
#undef init_color
#endif
int (*foo)(short, short, short, short) = init_color;
int main(void) { return 0; }

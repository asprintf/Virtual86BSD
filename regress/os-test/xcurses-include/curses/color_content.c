/*[SUSv2]*/
#include <curses.h>
#ifdef color_content
#undef color_content
#endif
int (*foo)(short, short *, short *, short *) = color_content;
int main(void) { return 0; }

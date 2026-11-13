/*[SUSv2]*/
#include <curses.h>
#ifdef pair_content
#undef pair_content
#endif
int (*foo)(short, short *, short *) = pair_content;
int main(void) { return 0; }

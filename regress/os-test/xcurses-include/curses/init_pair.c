/*[SUSv2]*/
#include <curses.h>
#ifdef init_pair
#undef init_pair
#endif
int (*foo)(short, short, short) = init_pair;
int main(void) { return 0; }

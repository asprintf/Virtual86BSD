/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attr_set
#undef slk_attr_set
#endif
int (*foo)(const attr_t, short, void *) = slk_attr_set;
int main(void) { return 0; }

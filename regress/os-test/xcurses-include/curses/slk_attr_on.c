/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attr_on
#undef slk_attr_on
#endif
int (*foo)(const attr_t, void *) = slk_attr_on;
int main(void) { return 0; }

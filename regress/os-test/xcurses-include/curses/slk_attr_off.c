/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attr_off
#undef slk_attr_off
#endif
int (*foo)(const attr_t, void *) = slk_attr_off;
int main(void) { return 0; }

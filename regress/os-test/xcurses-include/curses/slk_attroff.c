/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attroff
#undef slk_attroff
#endif
int (*foo)(const chtype) = slk_attroff;
int main(void) { return 0; }

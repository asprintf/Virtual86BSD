/*[SUSv2]*/
#include <curses.h>
#ifdef slk_attrset
#undef slk_attrset
#endif
int (*foo)(const chtype) = slk_attrset;
int main(void) { return 0; }

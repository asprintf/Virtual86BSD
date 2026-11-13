/*[SUSv2]*/
#include <curses.h>
#ifdef slk_set
#undef slk_set
#endif
int (*foo)(int, const char *, int) = slk_set;
int main(void) { return 0; }

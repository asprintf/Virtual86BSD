/*[SUSv2]*/
#include <curses.h>
#ifdef slk_label
#undef slk_label
#endif
char *(*foo)(int) = slk_label;
int main(void) { return 0; }

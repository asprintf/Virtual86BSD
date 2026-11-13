/*[SUSv2]*/
#include <curses.h>
#ifdef has_colors
#undef has_colors
#endif
bool (*foo)(void) = has_colors;
int main(void) { return 0; }

/*[SUSv2]*/
#include <curses.h>
#ifdef can_change_color
#undef can_change_color
#endif
bool (*foo)(void) = can_change_color;
int main(void) { return 0; }

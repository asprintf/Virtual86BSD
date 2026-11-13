/*[SUSv2]*/
#include <curses.h>
#ifdef has_ic
#undef has_ic
#endif
bool (*foo)(void) = has_ic;
int main(void) { return 0; }

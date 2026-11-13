/*[SUSv2]*/
#include <curses.h>
#ifdef has_il
#undef has_il
#endif
bool (*foo)(void) = has_il;
int main(void) { return 0; }

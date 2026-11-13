/*[SUSv2]*/
#include <curses.h>
#ifdef isendwin
#undef isendwin
#endif
bool (*foo)(void) = isendwin;
int main(void) { return 0; }

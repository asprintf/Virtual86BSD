/*[SUSv2]*/
#include <curses.h>
#ifdef immedok
#undef immedok
#endif
void (*foo)(WINDOW *, bool) = immedok;
int main(void) { return 0; }

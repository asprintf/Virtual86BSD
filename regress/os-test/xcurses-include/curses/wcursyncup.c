/*[SUSv2]*/
#include <curses.h>
#ifdef wcursyncup
#undef wcursyncup
#endif
void (*foo)(WINDOW *) = wcursyncup;
int main(void) { return 0; }

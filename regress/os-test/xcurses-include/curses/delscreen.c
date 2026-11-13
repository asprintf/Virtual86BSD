/*[SUSv2]*/
#include <curses.h>
#ifdef delscreen
#undef delscreen
#endif
void (*foo)(SCREEN *) = delscreen;
int main(void) { return 0; }

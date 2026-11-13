/*[SUSv2]*/
#include <curses.h>
#ifdef wbkgrndset
#undef wbkgrndset
#endif
void (*foo)(WINDOW *, const cchar_t *) = wbkgrndset;
int main(void) { return 0; }

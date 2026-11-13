/*[SUSv2]*/
#include <curses.h>
#ifdef bkgrndset
#undef bkgrndset
#endif
void (*foo)(const cchar_t *) = bkgrndset;
int main(void) { return 0; }

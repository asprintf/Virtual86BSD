/*[SUSv2]*/
#include <curses.h>
#ifdef qiflush
#undef qiflush
#endif
void (*foo)(void) = qiflush;
int main(void) { return 0; }

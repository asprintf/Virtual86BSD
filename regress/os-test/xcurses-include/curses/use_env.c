/*[SUSv2]*/
#include <curses.h>
#ifdef use_env
#undef use_env
#endif
void (*foo)(bool) = use_env;
int main(void) { return 0; }

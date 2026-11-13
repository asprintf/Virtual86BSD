/*[SUSv2]*/
#include <term.h>
#ifdef set_curterm
#undef set_curterm
#endif
TERMINAL *(*foo)(TERMINAL *) = set_curterm;
int main(void) { return 0; }

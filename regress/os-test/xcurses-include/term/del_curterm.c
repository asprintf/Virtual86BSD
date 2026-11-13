/*[SUSv2]*/
#include <term.h>
#ifdef del_curterm
#undef del_curterm
#endif
int (*foo)(TERMINAL *) = del_curterm;
int main(void) { return 0; }

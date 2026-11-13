/*[SUSv2]*/
#include <curses.h>
#ifdef instr
#undef instr
#endif
int (*foo)(char *) = instr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <term.h>
#ifdef tgetflag
#undef tgetflag
#endif
int (*foo)(char *) = tgetflag;
int main(void) { return 0; }

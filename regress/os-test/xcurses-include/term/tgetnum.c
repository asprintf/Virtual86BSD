/*[SUSv2]*/
#include <term.h>
#ifdef tgetnum
#undef tgetnum
#endif
int (*foo)(char *) = tgetnum;
int main(void) { return 0; }

/*[SUSv2]*/
#include <term.h>
#ifdef tgetent
#undef tgetent
#endif
int (*foo)(char *, const char *) = tgetent;
int main(void) { return 0; }

/*[SUSv2]*/
#include <term.h>
#ifdef tgetstr
#undef tgetstr
#endif
char *(*foo)(char *, char **) = tgetstr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <term.h>
#ifdef tgoto
#undef tgoto
#endif
char *(*foo)(char *, int, int) = tgoto;
int main(void) { return 0; }

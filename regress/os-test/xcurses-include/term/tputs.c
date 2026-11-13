/*[SUSv2]*/
#include <term.h>
#ifdef tputs
#undef tputs
#endif
int (*foo)(const char *, int, int (*)(int)) = tputs;
int main(void) { return 0; }

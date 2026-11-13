/*[SUSv2]*/
#include <term.h>
#ifdef putp
#undef putp
#endif
int (*foo)(const char *) = putp;
int main(void) { return 0; }

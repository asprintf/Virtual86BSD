/*[SUSv2]*/
#include <unctrl.h>
#ifdef unctrl
#undef unctrl
#endif
char *(*foo)(chtype) = unctrl;
int main(void) { return 0; }

/*[SUSv2]*/
#include <xti.h>
#ifdef t_getstate
#undef t_getstate
#endif
int (*foo)(int) = t_getstate;
int main(void) { return 0; }

/*[SUSv2]*/
#include <xti.h>
#ifdef t_unbind
#undef t_unbind
#endif
int (*foo)(int) = t_unbind;
int main(void) { return 0; }

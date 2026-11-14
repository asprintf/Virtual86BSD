/*[SUSv2]*/
#include <xti.h>
#ifdef t_sysconf
#undef t_sysconf
#endif
int (*foo)(int) = t_sysconf;
int main(void) { return 0; }

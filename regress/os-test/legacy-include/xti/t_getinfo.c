/*[SUSv2]*/
#include <xti.h>
#ifdef t_getinfo
#undef t_getinfo
#endif
int (*foo)(int, struct t_info *) = t_getinfo;
int main(void) { return 0; }

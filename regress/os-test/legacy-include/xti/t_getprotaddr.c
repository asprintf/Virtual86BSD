/*[SUSv2]*/
#include <xti.h>
#ifdef t_getprotaddr
#undef t_getprotaddr
#endif
int (*foo)(int, struct t_bind *, struct t_bind *) = t_getprotaddr;
int main(void) { return 0; }

/*[SUSv2]*/
#include <xti.h>
#ifdef t_accept
#undef t_accept
#endif
int (*foo)(int, int, const struct t_call *) = t_accept;
int main(void) { return 0; }

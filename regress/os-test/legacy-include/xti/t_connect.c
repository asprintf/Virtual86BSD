/*[SUSv2]*/
#include <xti.h>
#ifdef t_connect
#undef t_connect
#endif
int (*foo)(int, const struct t_call *, struct t_call *) = t_connect;
int main(void) { return 0; }

/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvconnect
#undef t_rcvconnect
#endif
int (*foo)(int, struct t_call *) = t_rcvconnect;
int main(void) { return 0; }

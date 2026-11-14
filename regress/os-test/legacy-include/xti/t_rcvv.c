/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvv
#undef t_rcvv
#endif
int (*foo)(int, struct t_iovec *, unsigned int, int *) = t_rcvv;
int main(void) { return 0; }

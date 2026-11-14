/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvvudata
#undef t_rcvvudata
#endif
int (*foo)(int, struct t_unitdata *, struct t_iovec *, unsigned int, int *) = t_rcvvudata;
int main(void) { return 0; }

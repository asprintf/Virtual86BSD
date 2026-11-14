/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvudata
#undef t_rcvudata
#endif
int (*foo)(int, struct t_unitdata *, int *) = t_rcvudata;
int main(void) { return 0; }

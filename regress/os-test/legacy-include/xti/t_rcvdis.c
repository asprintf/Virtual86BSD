/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvdis
#undef t_rcvdis
#endif
int (*foo)(int, struct t_discon *) = t_rcvdis;
int main(void) { return 0; }

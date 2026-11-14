/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcv
#undef t_rcv
#endif
int (*foo)(int, void *, unsigned int, int *) = t_rcv;
int main(void) { return 0; }

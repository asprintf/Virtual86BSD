/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvreldata
#undef t_rcvreldata
#endif
int (*foo)(int, struct t_discon *) = t_rcvreldata;
int main(void) { return 0; }

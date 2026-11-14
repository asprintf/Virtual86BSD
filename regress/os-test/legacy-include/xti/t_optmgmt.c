/*[SUSv2]*/
#include <xti.h>
#ifdef t_optmgmt
#undef t_optmgmt
#endif
int (*foo)(int, const struct t_optmgmt *, struct t_optmgmt *) = t_optmgmt;
int main(void) { return 0; }

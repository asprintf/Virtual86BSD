/*[SUSv2]*/
#include <xti.h>
#ifdef t_sndudata
#undef t_sndudata
#endif
int (*foo)(int, const struct t_unitdata *) = t_sndudata;
int main(void) { return 0; }

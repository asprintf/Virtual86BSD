/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvuderr
#undef t_rcvuderr
#endif
int (*foo)(int, struct t_uderr *) = t_rcvuderr;
int main(void) { return 0; }

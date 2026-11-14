/*[SUSv2]*/
#include <xti.h>
#ifdef t_rcvrel
#undef t_rcvrel
#endif
int (*foo)(int) = t_rcvrel;
int main(void) { return 0; }

/*[SUSv2]*/
#include <xti.h>
#ifdef t_look
#undef t_look
#endif
int (*foo)(int) = t_look;
int main(void) { return 0; }
